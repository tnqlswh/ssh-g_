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
#include <signal.h>

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
	void(*effect)(int* hp, int *def, int *dmg);
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
    printf("주사위를 돌렸습니다.\n\n");

	return dice;
}

//서포트 카드 생성하는 함수
SupportCard create_card(const char *name, int black, int white, void(*effect)(int* hp, int *def, int *dmg))
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

/*
void act_init(GameAct* game_act) {
	
	game_act->round = 1;

	game_act->act_string[0] = '\0';

	game_act->cy_pass = 0;
	game_act->tn_pass = 0;
}
*/
void shield_effect(int* hp, int *def, int *dmg)
{
	*def += 5;
}

void powerup_effect(int* hp, int* def, int* dmg)
{
	*dmg += 5;
}

void heal_effect(int* hp, int* def, int* dmg)
{
	*hp += 5;
}


int turn = 0;
int first_round = 1;
int cy_pass = 0;
int tn_pass = 0;

void enemy_card() {
	turn = 0;
	tn_pass = 0;
	printf("\nB used card!\n\n");
}


void enemy_atk() {
	turn = 0;
	tn_pass = 0;
	printf("\nB attacked A!\n\n");
}

void pass() {
	turn = 0;
	tn_pass = 1; 
	printf("\nB passed turn!\n\n");
}


void skip() {
	return; 
}

int main()
{	
	signal(SIGCONT, enemy_card);
	signal(SIGUSR1, enemy_atk);
	signal(SIGUSR2, skip);

	int signo = 0;
	int act = 0; // 플레이어 행동 입력 받기용 변수 
	int card_size = 0; // 현재 카드 몇장인지 확인용 
	char act_string[MAX_CHAR];

	//서포트 카드 설정
	SupportCard cards[] = {
		create_card("shield", 0, 2, shield_effect),
		create_card("powerup", 1, 1, powerup_effect),
		create_card("heal", 3, 1, heal_effect)
	};

	int shmid;
	key_t key;
	GameState *game_state;
	GameState temp_state;

	key = ftok("cytn", 4272);
	shmid = shmget(key, sizeof(GameState), IPC_CREAT|0666);
	if(shmid == -1)
	{
		perror("shmget");
		exit(1);
	}
	game_state = (GameState *)shmat(shmid, NULL, 0);
	state_init(game_state);
	temp_state = *game_state;



	int tn_pid;
	
	printf("내 PID : %d\n", getpid());
	printf("상대의 PID를 입력해주세요.\ninput: ");
	scanf("%d", &tn_pid);

	kill(tn_pid, SIGUSR2);
	pause();

	printf("\nGAME START!\n\n");
	
	signal(SIGUSR2, pass);

	int start = 0;
	SupportCard get_card[MAX_CARD];
	Dice get_dice;

	while (1)
	{
		if (turn == 0) {

			if (start != 0) {
				temp_state = *game_state;
			}
			start = 1;
			cy_pass = 0;

			printf("A hp : %d\n", temp_state.cyno_hp);
			printf("A def : %d\n", temp_state.cyno_def);
			printf("A dmg : %d\n\n", temp_state.cyno_dmg);
			printf("B hp : %d\n", temp_state.tighnari_hp);
			printf("B def : %d\n", temp_state.tighnari_def);
			printf("B dmg : %d\n\n", temp_state.tighnari_dmg);

			// 새 라운드 시작 
			if (first_round == 1) {
				int fd[2];
				pid_t cardDice;

				pipe(fd);
				card_size = MAX_CARD;
				cardDice = fork();
				// 자식 프로세스 
				if (cardDice == 0)
				{
					SupportCard selected_cards[MAX_CARD];

					Dice dice = rolling_dice();
					random_card(cards, selected_cards);

					write(fd[1], &dice, sizeof(dice));
					write(fd[1], &selected_cards, sizeof(selected_cards));
					close(fd[1]);
					exit(1);
				}

				else {
					close(fd[1]);

					read(fd[0], &get_dice, sizeof(get_dice));
					read(fd[0], &get_card, sizeof(get_card));
					close(fd[0]);

				}
			}	


			printf("백 주사위: %d\n흑 주사위: %d\n\n", get_dice.white_dice, get_dice.black_dice);
			printf("-사용 가능한 서포트 카드\n");
			for (int i = 0; i < card_size; i++)
			{
				printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);
			}
			printf("\n");

			while (1) {

				printf("1. 카드 선택\n2. 공격\n3. 턴 종료\ninput: ");
				scanf("%d", &act);
				printf("\n");

				if (act == 1) {

					printf("-사용 가능한 서포트 카드\n");

					for (int i = 0; i < card_size; i++)
					{
						printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);

					}
					printf("\n");
					printf("input: ");
					scanf("%d", &act);

					if (get_dice.white_dice < get_card[act - 1].white_cost || get_dice.black_dice < get_card[act - 1].black_cost)
						printf("주사위가 부족합니다.\n\n");

					else {
						
						get_dice.white_dice -= get_card[act - 1].white_cost;
						get_dice.black_dice -= get_card[act - 1].black_cost;
						
						get_card[act - 1].effect(&temp_state.cyno_hp, &temp_state.cyno_def, &temp_state.cyno_dmg);
						printf("%s 카드를 사용했습니다.\n\n", get_card[act - 1].name);

						if (act == 1) 
							get_card[act - 1] = get_card[act];


						card_size -= 1;

						*game_state = temp_state;
						signo = SIGCONT;

						break;
					}

				}

				else if (act == 2) {

					printf("사용할 주사위를 선택하세요.\n1. 백\n2. 흑\ninput: ");
					scanf("%d", &act);

					if ((act == 1 && get_dice.white_dice > 0) || (act == 2 && get_dice.black_dice > 0)) {
						
						if (act == 1) get_dice.white_dice -= 1;
						else if (act == 2) get_dice.black_dice -= 1;

						int dmg = 10 + temp_state.cyno_dmg - temp_state.tighnari_def;
						
						temp_state.tighnari_hp -= dmg;

						printf("상대를 공격했습니다.\nDMG: %d\n\n", dmg);
						signo = SIGUSR1;

						*game_state = temp_state;
						break;

					}

					else {
						printf("주사위가 부족합니다.\n\n");
					}
					
				}

				else if (act == 3) {

					printf("턴을 넘깁니다.\n\n");
					
					cy_pass = 1;
					signo = SIGUSR2;
					break;
							
				}
				
			}

			if (cy_pass == 1 && tn_pass == 1) {
				printf("\nNEW ROUND!\n\n");
				first_round = 1;
				kill(tn_pid, SIGUSR2);
				pause();
				continue;
			}
			
			if (temp_state.tighnari_hp <= 0) {
				printf("\nYOU WIN!\n\n");
				kill(tn_pid, SIGUSR1);
				exit(1);
			}

			first_round += 1;

			printf("\nwaiting. . .\n\n");
			kill(tn_pid, signo);
			pause();
			
			if (game_state->cyno_hp <= 0) {
				printf("\nYOU LOSE . . . \n\n");
				exit(1);
			}
			

		}
	}
	
	

	return 0;
}
