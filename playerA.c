#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <string.h>

#define MAX_DICE 6
#define MAX_CARD 2
#define MAX_CHAR 500

typedef struct //플레이어 상태
{
	int cyno_hp;
	int cyno_def;
	int cyno_dmg;

	int tighnari_hp;
	int tighnari_def;
	int tighnari_dmg;

	int current_turn; //0은 사이노 1은 타이나리

} GameState;

typedef struct {

	int round; // 몇번째 턴인지 

	pid_t cy_pid; // 각 프로세스 pid
	pid_t tn_pid;

	char act_string[MAXCHAR]; // 행동 담아둘 배열 

	int cy_pass; // 패스했는지 확인
	int tn_pass; 

}GameAct;

typedef struct //주사위
{
	int diced[MAX_DICE]; //주사위 배열
	int white_dice;
	int black_dice;
} Dice;

typedef struct //서포트 카드
{
	GameState *game_state;
	Dice *dice;
	char name[32];
	int black_cost;
	int white_cost;
	void(*effect)(int *def, int *dmg);
} SupportCard;

//주사위 랜덤으로 굴리는 함수
Dice rolling_dice()
{
	Dice dice;
    dice.white_dice = 0;
    dice.black_dice = 0;

	srand(time(NULL));
    for(int i = 0; i < MAX_DICE; i++)
    {
        dice.diced[i] = rand()%2;

		if(dice.diced[i] == 0)
	    {
			dice.white_dice++;
        }
        else if(dice.diced[i] == 1)
        {
            dice.black_dice++;
        }
    }
    printf("주사위를 돌렸습니다.\n");

	return dice;
}

//서포트 카드 생성하는 함수
SupportCard create_card(const char *name, int black, int white, void(*effect)(int *def, int *dmg))
{
	SupportCard card;
	strcpy(card.name, name);
	card.black_cost = black;
	card.white_cost = white;
	card.effect = effect;
	return card;
}

//서포트 카드 랜덤으로 뽑는 함수
void random_card(SupportCard cards[], SupportCard selected_card[])
{
	srand(time(NULL));
	for(int i = 0; i < MAX_CARD; i++)
	{
		selected_card[i] = cards[rand()%3]; //rand 숫자는 서포트 카드개수 따라 적을 것
	}
}

//플레이어 상태 초기화 함수
void state_init(GameState* game_state)
{
	game_state->current_turn = 0;

	game_state->cyno_hp = 50;
	game_state->cyno_def = 0;
	game_state->cyno_dmg = 0;

	game_state->tighnari_hp = 50;
	game_state->tighnari_def = 0;
	game_state->tighnari_dmg = 0;

}

void act_init(GameAct* game_act) {
	
	game_act->round = 1;

	game_act->act_string[0] = '\0';

	cy_pass = 0;
	tn_pass = 0;
}

void shield_effect(int *def, int *dmg)
{
	*def += 5;
}

void play_turn(GameState *game_state, int player_id)
{

}

void change_turn(GameState* game_state) {
	game_state->current_turn = 1;
}

void pass_turn(GameAct* game_act) {
	game_act->cy_pass = 1; 
}

int main()
{	

	int act = 0; // 플레이어 행동 입력 받기용 변수 
	int card_size = 0; // 현재 카드 몇장인지 확인용 
	char act_string[MAX_CHAR];

	//서포트 카드 설정
	SupportCard cards[] = {
		create_card("shild", 0, 2, shield_effect),
		create_card("test", 1, 1, shield_effect),
		create_card("test2", 3, 1, shield_effect)
	};

	int shmid;
	key_t key;
	GameState *game_state;

	key = ftok("cytn", 4272);
	shmid = shmget(key, sizeof(GameState), IPC_CREAT|0666);
	if(shmid == -1)
	{
		perror("shmget");
		exit(1);
	}
	game_state = (GameState *)shmat(shmid, NULL, 0);

	state_init(game_state);

	// 새로 만든 공유 메모리 
	int shmid_act;
	key_t key_act;
	GameAct* game_act;

	key_act = ftok("tncy", 7242);
	shmid_act = shmget(key_act, sizeof(GameAct), IPC_CREAT | 0666);
	if (shmid == -1)
	{
		perror("shmget");
		exit(1);
	}
	game_act = (GameAct*)shmat(shmid_act, NULL, 0);

	act_init(game_act);

	// pid 저장 
	game_act->cy_pid = getpid();

	signal(SIGUSR1, change_turn(game_state));
	signal(SIGUSR2, pass_turn(game_act));

	int fd[2];
	pid_t cardDice;

	pipe(fd);
	cardDice = fork();

	while (true)
	{
		if (game_state->current_turn == 0) {

			if (game_act->round == 1) {

				card_size = MAX_CARD;

				if (cardDice == 0)
				{
					SupportCard selected_cards[MAX_CARD];

					Dice dice = rolling_dice();
					random_card(cards, selected_cards);

					write(fd[1], &dice, sizeof(dice));
					write(fd[1], &selected_cards, sizeof(selected_cards));
					close(fd[1]);
				}
				else
				{
					close(fd[1]);

					SupportCard get_card[MAX_CARD];
					Dice get_dice;

					read(fd[0], &get_dice, sizeof(get_dice));
					read(fd[0], &get_card, sizeof(get_card));
					close(fd[0]);

					printf("흰 주사위: %d\n흑 주사위: %d\n", get_dice.white_dice, get_dice.black_dice);
					printf("-사용 가능한 서포트 카드\n");
					for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
					{
						printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);
					}

					while (true) {

						printf("1. 카드 선택\n2. 공격\n3. 턴 종료\ninput: ");
						scanf("%d", &act);

						if (act == 1) {

							printf("-사용 가능한 서포트 카드\n");

							for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
							{
								printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);

							}

							printf("input: ");
							scanf("%d", &act);

							if (get_dice.white_dice < get_card[act - 1].white_cost || get_dice.black_dice < get_card[act - 1].black_cost)
								printf("주사위가 부족합니다.\n");

							else {
								// 이거 되는지 잘 모르겠음 
								strcpy(act_string, "playerA used %s\n", get_card[act - 1].name);
								strcpy(game_act.act_string, act_string);
								// 카드 효과 적용, 카드 삭제 써야 함 
							}

						}

						else if (act == 2) {

							printf("상대를 때립니다. 아야~");
							strcpy(act_string, "playerA attacked playerB\n");
							act = 3;
							kill(game_act->tn_pid, SIGUSR1);

						}

						else if (act == 3) {

							// 다 하고 선택한 건지 아니면 그냥 냅다 패스한건지 확인해야 함 
							kill(game_act->tn_pid, SIGUSR2);
							kill(game_act->tn_pid, SIGUSR1);
							// 이거 하다가 갑자기 혼란 옴 굳이 이럴 필요 없지 않나 공유메모리 쓰는데 
						}

					}
				}
			}

		}
	}
	
	

	return 0;
}
