#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>


#define ALTURA 450
#define LARGURA 800
#define VELOCIDADE_MAXIMA 900.0f
#define PONTOS_VITORIA 5
#define ALTURA_BARRA 80.0f
#define ALTURA_BARRA_GRANDE 140.0f
#define TEMPO_APARECER 2.0f     // segundos até o item aparecer
#define DURACAO_EFEITO 8.0f     // segundos que o efeito dura
#define ALTURA_BARRA_PEQUENA 40.0f
#define TEMPO_VIDA_ITEM 5.0f    // segundos que o item fica na tela se ninguém pegar
#define DURACAO_INVERSAO 5.0f   // segundos de controles invertidos
#define FATOR_BOLA_LENTA 0.5f   // 0.5 = a bola anda na metade da velocidade



typedef struct{
    Vector2 pos;
    Vector2 vel;
    float raio;
} Bola;

typedef struct{
    Rectangle ret;
    float vel;
    int pontos;
    float tempoEfeito;   // segundos restantes do power-up (0 = sem efeito)
    float tempoInvertido;  // segundos restantes de controles invertidos (0 = normal)
} Barra;

typedef enum {
    PU_BARRA_GRANDE,       // aumenta a barra de quem pegou
    PU_ENCOLHE_OPONENTE,   // diminui a barra do oponente
    PU_BOLA_LENTA,         // a bola fica mais lenta por um tempo
    PU_INVERTE_OPONENTE,   // inverte os controles do oponente
    PU_QUANTIDADE          // truque: sempre o último, vale quantos tipos existem
} TipoPowerUp;

typedef struct{
    Vector2 pos;        // centro do item
    float raio;
    bool ativo;         // true = está na tela
    TipoPowerUp tipo;   // qual efeito ele dá
    float tempoVida;    // segundos restantes até ele sumir sozinho
} PowerUp;



typedef enum { MENU, DIFICULDADE, CONTROLES, JOGANDO, FIM } Estado;
typedef enum { DOIS_JOGADORES, CONTRA_IA } Modo;

typedef struct {
    const char *nome;
    float velocidade;  // fracao da velocidade de uma barra humana
    float zonaMorta;   // tolerancia em pixels
    float reacao;      // a partir de que ponto da tela a IA comeca a se mexer
} ConfigIA;

static const ConfigIA CONFIG_IA[3] = {
    { "Facil",   0.40f, 40.0f, 0.60f },
    { "Normal",  0.60f, 15.0f, 0.40f },
    { "Dificil", 0.80f,  5.0f, 0.00f },
};


static void ColideBarraBola(Bola *b, const Barra *br, int direcao){
    float centro = br->ret.y + br->ret.height / 2.0f;
    float offset = (b->pos.y - centro) / (br->ret.height / 2.0f);
    
    b->vel.x = fabsf(b->vel.x) * 1.05f * direcao;

    if (b->vel.x > VELOCIDADE_MAXIMA) b->vel.x = VELOCIDADE_MAXIMA;
    if (b->vel.x < -VELOCIDADE_MAXIMA) b->vel.x = -VELOCIDADE_MAXIMA;

    b->vel.y = 300.0f * offset;

    if (direcao == 1){
        b->pos.x = br->ret.x + br->ret.width + b->raio;
    } else {
        b->pos.x = br->ret.x - b->raio;
    }
}
static void ResetaBola(Bola *b, int direcao){
    b->pos = (Vector2){ LARGURA / 2.0f, ALTURA / 2.0f };
    b->vel = (Vector2){ 300.0f * direcao, 300.0f * (rand() % 2 == 0 ? 1 : -1) };
}
static void DrawTextCentro(const char *txt, int y, int tam, Color cor){
    DrawText(txt, (LARGURA - MeasureText(txt, tam)) / 2, y, tam, cor);
}

// Muda a altura da barra crescendo/encolhendo a partir do CENTRO
static void DefineAlturaBarra(Barra *br, float novaAltura){
    float centro = br->ret.y + br->ret.height / 2.0f;
    br->ret.height = novaAltura;
    br->ret.y = centro - novaAltura / 2.0f;
}

// Faz o cronômetro do efeito andar; quando zera, a barra volta ao normal
static void AtualizaEfeito(Barra *br, float dt){
    if (br->tempoEfeito > 0){
        br->tempoEfeito -= dt;
        if (br->tempoEfeito <= 0){
            DefineAlturaBarra(br, ALTURA_BARRA);
        }
    }
}

// Cor da barra: branca normal, verde se está grande, vermelha se está pequena
// Cor da barra: roxa se invertida, branca normal, verde se grande, vermelha se pequena
static Color CorBarra(const Barra *br){
    if (br->tempoInvertido > 0) return PURPLE;
    if (br->tempoEfeito <= 0) return WHITE;
    return (br->ret.height > ALTURA_BARRA) ? GREEN : RED;
}

// Cor de cada tipo de item
static Color CorItem(TipoPowerUp t){
    switch (t) {
        case PU_BARRA_GRANDE:     return GREEN;
        case PU_ENCOLHE_OPONENTE: return RED;
        case PU_BOLA_LENTA:       return SKYBLUE;
        case PU_INVERTE_OPONENTE: return PURPLE;
        default:                  return WHITE;
    }
}

// Símbolo desenhado dentro de cada tipo de item
static const char *SimboloItem(TipoPowerUp t){
    switch (t) {
        case PU_BARRA_GRANDE:     return "+";
        case PU_ENCOLHE_OPONENTE: return "-";
        case PU_BOLA_LENTA:       return "L";
        case PU_INVERTE_OPONENTE: return "?";
        default:                  return "";
    }
}


static void ReiniciaPartida(Barra *j1, Barra *j2, Bola *bola){
    j1->pontos = 0;
    j2->pontos = 0;
    j1->tempoEfeito = 0;
    j2->tempoEfeito = 0;
    j1->tempoInvertido = 0;
    j2->tempoInvertido = 0;
    DefineAlturaBarra(j1, ALTURA_BARRA);
    DefineAlturaBarra(j2, ALTURA_BARRA);
    j1->ret.y = ALTURA / 2.0f - 40;
    j2->ret.y = ALTURA / 2.0f - 40;
    ResetaBola(bola, 1);
}


int main (void){
    srand(time(NULL));
    SetRandomSeed((unsigned int)time(NULL));   // semente do GetRandomValue da raylib
    InitWindow(LARGURA,ALTURA,"Pong");
    SetTargetFPS(60);

    Barra j1 = { { 20, ALTURA / 2.0f - 40, 12, 80}, 350.0f, 0};
    Barra j2 = { { LARGURA - 32, ALTURA / 2.0f - 40, 12, 80}, 350.0f, 0};
    Bola bola = {0};
    bola.raio = 8.0f;
    ResetaBola(&bola, 1);
    Estado estado = MENU;
    Modo modo = DOIS_JOGADORES;
    int opcao = 0;      // 0 = 2 jogadores, 1 = contra a IA
    int dif = 1;        // dificuldade: 0 = Facil, 1 = Normal, 2 = Dificil
    int vencedor = 0;
    PowerUp pu = {0};
    pu.raio = 14.0f;
    float tempoParaPU = TEMPO_APARECER;
    int ultimoRebatedor = 0;   // 0 = ninguém ainda, 1 = jogador 1, 2 = jogador 2
    float tempoBolaLenta = 0;  // segundos restantes de bola lenta (0 = normal)
    
    while(!WindowShouldClose()){
        float dt = GetFrameTime();

        switch (estado) {

        case MENU:
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) opcao = 0;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) opcao = 1;

            if (IsKeyPressed(KEY_ENTER)) {
                if (opcao == 0) {
                    modo = DOIS_JOGADORES;
                    ReiniciaPartida(&j1, &j2, &bola);
                    vencedor = 0;
                    estado = CONTROLES;
                } else {
                    modo = CONTRA_IA;
                    estado = DIFICULDADE;
                }
            }
            break;

        case DIFICULDADE:
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) dif--;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) dif++;
            if (dif < 0) dif = 0;
            if (dif > 2) dif = 2;

            if (IsKeyPressed(KEY_ENTER)) {
                ReiniciaPartida(&j1, &j2, &bola);
                vencedor = 0;
                estado = CONTROLES;
            }
            if (IsKeyPressed(KEY_BACKSPACE)) estado = MENU;
            break;

            case CONTROLES:
                if (IsKeyPressed(KEY_ENTER)) {
                    tempoBolaLenta = 0;
                    pu.ativo = false;
                    tempoParaPU = TEMPO_APARECER;
                    ultimoRebatedor = 0;
                    estado = JOGANDO;
            }
            if (IsKeyPressed(KEY_BACKSPACE)) estado = MENU;
            break;

        case JOGANDO: {
            const ConfigIA *cfg = &CONFIG_IA[dif];
            // cronômetros dos controles invertidos e direção de cada jogador
            if (j1.tempoInvertido > 0) j1.tempoInvertido -= dt;
            if (j2.tempoInvertido > 0) j2.tempoInvertido -= dt;
            float dir1 = (j1.tempoInvertido > 0) ? -1.0f : 1.0f;
            float dir2 = (j2.tempoInvertido > 0) ? -1.0f : 1.0f;
            // 1 - Entrada
            // jogador 1: sempre W / S
            if (IsKeyDown(KEY_W)) j1.ret.y -= j1.vel * dt * dir1;
            if (IsKeyDown(KEY_S)) j1.ret.y += j1.vel * dt * dir1;

            // jogador 2: humano ou IA, dependendo do modo
            if (modo == DOIS_JOGADORES) {
                if (IsKeyDown(KEY_UP))   j2.ret.y -= j2.vel * dt * dir2;
                if (IsKeyDown(KEY_DOWN)) j2.ret.y += j2.vel * dt * dir2;
            } else {
                float centroIA = j2.ret.y + j2.ret.height / 2.0f;
                float velIA    = j2.vel * cfg->velocidade;

                // só reage se a bola vem na direção dela e já passou do ponto de reação
                if (bola.vel.x > 0 && bola.pos.x > LARGURA * cfg->reacao) {
                    if (bola.pos.y < centroIA - cfg->zonaMorta)      j2.ret.y -= velIA * dt * dir2;
                    else if (bola.pos.y > centroIA + cfg->zonaMorta) j2.ret.y += velIA * dt * dir2;
                }
            }

            // 2 - Atualizacao
            AtualizaEfeito(&j1, dt);
            AtualizaEfeito(&j2, dt);
            if(j1.ret.y < 0) j1.ret.y = 0;
            if(j1.ret.y + j1.ret.height > ALTURA) j1.ret.y = ALTURA - j1.ret.height;
            if(j2.ret.y < 0) j2.ret.y = 0;
            if(j2.ret.y + j2.ret.height > ALTURA) j2.ret.y = ALTURA - j2.ret.height;

            // bola lenta: enquanto o efeito durar, a bola anda numa fração da velocidade
            float fatorBola = 1.0f;
            if (tempoBolaLenta > 0) {
                tempoBolaLenta -= dt;
                fatorBola = FATOR_BOLA_LENTA;
            }

            //Movimento da bola = posicao = posicao + velocidade * tempo
            bola.pos.x += bola.vel.x * dt * fatorBola;
            bola.pos.y += bola.vel.y * dt * fatorBola;

            //rebate a bola no teto e no chao

            if(bola.pos.y - bola.raio < 0){
                bola.pos.y = bola.raio;
                bola.vel.y = -bola.vel.y;
            }
            if(bola.pos.y + bola.raio > ALTURA){
                bola.pos.y = ALTURA - bola.raio;
                bola.vel.y = -bola.vel.y;
            }

            //colisao com as barras

            if (CheckCollisionCircleRec(bola.pos, bola.raio, j1.ret)) {
                ColideBarraBola(&bola, &j1, 1);
                ultimoRebatedor = 1;
            }
            if (CheckCollisionCircleRec(bola.pos, bola.raio, j2.ret)) {
                ColideBarraBola(&bola, &j2, -1);
                ultimoRebatedor = 2;
            }

            // --- power-up: aparecer / sumir sozinho ---

            if (!pu.ativo) {
                tempoParaPU -= dt;
                if (tempoParaPU <= 0) {
                    pu.ativo = true;
                    pu.tipo = (TipoPowerUp)GetRandomValue(0, PU_QUANTIDADE - 1);
                    pu.tempoVida = TEMPO_VIDA_ITEM;
                    pu.pos = (Vector2){ GetRandomValue(LARGURA / 4, LARGURA * 3 / 4),
                                        GetRandomValue(60, ALTURA - 60) };
                }
            } else {
                pu.tempoVida -= dt;
                if (pu.tempoVida <= 0) {
                    pu.ativo = false;
                    tempoParaPU = TEMPO_APARECER;
                }
            }

            // --- power-up: ser pego ---
        
            if (pu.ativo && ultimoRebatedor != 0 &&
                CheckCollisionCircles(bola.pos, bola.raio, pu.pos, pu.raio)) {
                Barra *quem  = (ultimoRebatedor == 1) ? &j1 : &j2;   // quem pegou
                Barra *outro = (ultimoRebatedor == 1) ? &j2 : &j1;   // o oponente

                switch (pu.tipo) {
                    case PU_BARRA_GRANDE:
                        DefineAlturaBarra(quem, ALTURA_BARRA_GRANDE);
                        quem->tempoEfeito = DURACAO_EFEITO;
                        break;
                    case PU_ENCOLHE_OPONENTE:
                        DefineAlturaBarra(outro, ALTURA_BARRA_PEQUENA);
                        outro->tempoEfeito = DURACAO_EFEITO;
                        break;
                    case PU_BOLA_LENTA:
                        tempoBolaLenta = DURACAO_EFEITO;
                        break;
                    case PU_INVERTE_OPONENTE:
                        outro->tempoInvertido = DURACAO_INVERSAO;
                        break;
                    default:
                        break;
                }
                pu.ativo = false;
                tempoParaPU = TEMPO_APARECER;
            }






            //pontuacao = se a bola sair pela esquerda ou pela direita
            if (bola.pos.x < 0) {
                j2.pontos++; ResetaBola(&bola, -1);
                ultimoRebatedor = 0;
            }
            if (bola.pos.x > LARGURA) {
                j1.pontos++; ResetaBola(&bola, +1);
                ultimoRebatedor = 0;
            }

            //verifica quem venceu
            if (j1.pontos >= PONTOS_VITORIA) { vencedor = 1; estado = FIM; }
            if (j2.pontos >= PONTOS_VITORIA) { vencedor = 2; estado = FIM; }
            break;
        }

        case FIM:
            if (IsKeyPressed(KEY_ENTER)) estado = MENU;
            break;
        }

        // 3 - Desenho
        BeginDrawing();
        ClearBackground(BLACK);

        if (estado == MENU) {
            DrawTextCentro("PONG", 70, 80, WHITE);
            DrawTextCentro("2 Jogadores", 220, 36, opcao == 0 ? YELLOW : GRAY);
            DrawTextCentro("Contra a IA",  280, 36, opcao == 1 ? YELLOW : GRAY);
            DrawTextCentro("W/S ou setas: escolher    ENTER: confirmar", 390, 20, GRAY);

        } else if (estado == DIFICULDADE) {
            DrawTextCentro("DIFICULDADE", 70, 60, WHITE);
            for (int i = 0; i < 3; i++)
                DrawTextCentro(CONFIG_IA[i].nome, 190 + i * 55, 36, i == dif ? YELLOW : GRAY);
            DrawTextCentro("W/S ou setas: escolher    ENTER: confirmar    BACKSPACE: voltar", 390, 18, GRAY);

        } else if (estado == CONTROLES) {
            DrawTextCentro("CONTROLES", 60, 60, WHITE);
            if (modo == DOIS_JOGADORES) {
                DrawTextCentro("Jogador 1 (esquerda):  W / S", 170, 30, WHITE);
                DrawTextCentro("Jogador 2 (direita):  setas CIMA / BAIXO", 220, 30, WHITE);
            } else {
                DrawTextCentro("Voce (esquerda):  W / S", 170, 30, WHITE);
                DrawTextCentro(TextFormat("IA (direita) - dificuldade: %s", CONFIG_IA[dif].nome),
                               220, 30, WHITE);
            }
            DrawTextCentro(TextFormat("Vence quem fizer %d pontos", PONTOS_VITORIA), 255, 24, GRAY);
            DrawTextCentro("Verde (+): sua barra cresce    Vermelho (-): encolhe a do oponente", 298, 18, GRAY);
            DrawTextCentro("Azul (L): bola lenta    Roxo (?): inverte os controles do oponente", 322, 18, GRAY);
            DrawTextCentro("Pega o item quem rebateu a bola por ultimo", 346, 18, GRAY);
            DrawTextCentro("ENTER: comecar    BACKSPACE: voltar", 395, 20, YELLOW);

        } else {  // JOGANDO ou FIM
            for (int y=0;y<ALTURA;y+=20){
                DrawRectangle(LARGURA /2-2,y,4,10,GRAY);
            }
            DrawRectangleRec(j1.ret, CorBarra(&j1));
            DrawRectangleRec(j2.ret, CorBarra(&j2));
            DrawCircleV(bola.pos, bola.raio, tempoBolaLenta > 0 ? SKYBLUE : WHITE);

            if (pu.ativo) {
                DrawCircleV(pu.pos, pu.raio, CorItem(pu.tipo));
                DrawCircleLines((int)pu.pos.x, (int)pu.pos.y, pu.raio, WHITE);
                const char *simbolo = SimboloItem(pu.tipo);
                DrawText(simbolo, (int)pu.pos.x - MeasureText(simbolo, 20) / 2,
                         (int)pu.pos.y - 10, 20, BLACK);
            }
            DrawText(TextFormat("%d",j1.pontos),LARGURA /4,20,40,WHITE);
            DrawText(TextFormat("%d",j2.pontos),LARGURA *3/4,20,40,WHITE);

            //mensagem de vitoria
            if (estado == FIM) {
                if (modo == CONTRA_IA && vencedor == 1)
                    DrawTextCentro("GAME OVER - Voce venceu!", ALTURA / 2 - 40, 40, YELLOW);
                else if (modo == CONTRA_IA && vencedor == 2)
                    DrawTextCentro("GAME OVER - A IA venceu!", ALTURA / 2 - 40, 40, YELLOW);
                else
                    DrawTextCentro(TextFormat("GAME OVER - Player %d wins!", vencedor),
                                   ALTURA / 2 - 40, 40, YELLOW);
                DrawTextCentro("ENTER: voltar ao menu", ALTURA / 2 + 20, 20, WHITE);
            }
        }
        EndDrawing();

    }
    CloseWindow();
    return 0;
   }