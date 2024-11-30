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

typedef struct
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

	char act_string[MAX_CHAR]; // 행동 담아둘 배열 

	int cy_pass; // 패스했는지 확인
	int tn_pass;

} GameAct;

typedef struct
{
	int diced[MAX_DICE]; //주사위 배열
	int white_dice;
	int black_dice;
} Dice;

typedef struct
{
	GameState *game_state;
	Dice *dice;
	char name[32];
	int black_cost;
	int white_cost;
	void(*effect)(int* hp, int *def, int *dmg);
} SupportCard;

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

SupportCard create_card(const char *name, int black, int white, void(*effect)(int* hp, int *def, int *dmg))
{
	SupportCard card;
	strcpy(card.name, name);
	card.black_cost = black;
	card.white_cost = white;
	card.effect = effect;
	return card;
}

void random_card(SupportCard cards[], SupportCard selected_card[])
{
	srand(time(NULL));
	for(int i = 0; i < MAX_CARD; i++)
	{
		selected_card[i] = cards[rand()%3]; //rand 숫자는 서포트 카드개수 따라 적을 것
	}
}

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

	game_act->cy_pass = 0;
	game_act->tn_pass = 0;
}

void shield_effect(int* hp, int* def, int* dmg)
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

void enemy_shield() {
	turn = 1;
	printf("\nA used shield!\n\n");
}

void enemy_powerup() {
	turn = 1;
	printf("\nA used powerup!\n\n");
}

void enemy_heal() {
	turn = 1;
	printf("\nA used heal!\n\n");
}

void enemy_atk() {
	turn = 1;
	printf("\nA attacked B!\n\n");
}

void pass() {
	turn = 1;
}

void test() {
	return;
}

int main()
{

	signal(SIGFPE, enemy_shield);
	signal(SIGTSTP, enemy_powerup);
	signal(SIGALRM, enemy_heal);
	signal(SIGUSR1, enemy_atk);
	signal(SIGUSR2, test);

	int signo;

	int act = 0; // 플레이어 행동 입력 받기용 변수 
	int card_size = 0; // 현재 카드 몇장인지 확인용 
	char act_string[MAX_CHAR];

	SupportCard cards[] = {
		create_card("shield", 0, 2, shield_effect),
		create_card("powerup", 1, 1, powerup_effect),
		create_card("heal", 3, 1, heal_effect)
	};

	printf("내 PID : %d\n", getpid());

	pause();

	int shmid;
	key_t key;
	GameState *game_state;
	GameState temp_state;

	key = ftok("cytn", 4272);
	shmid = shmget(key, sizeof(GameState), 0666);
	if(shmid == -1)
	{
		perror("shmget");
		exit(1);
	}

	game_state = (GameState *)shmat(shmid, NULL, 0);
	

	int shmid_act;
	key_t key_act;
	GameAct* game_act;

	key_act = ftok("tncy", 7242);
	shmid_act = shmget(key_act, sizeof(GameAct), 0666);
	if (shmid_act == -1)
	{
		perror("shmget");
		exit(1);
	}
	game_act = (GameAct *)shmat(shmid_act, NULL, 0);

	int fd[2];
	pid_t cardDice;

	pipe(fd);

	int cy_pid;

	printf("상대의 PID를 입력해주세요.\ninput: ");
	scanf("%d", &cy_pid);


	printf("\nGAME START!\n\n");

	printf("\nwaiting. . .\n\n");

	kill(cy_pid, SIGUSR2);
	pause();

	signal(SIGUSR2, pass);

	while (1)
	{
		if (turn == 1) {

			temp_state = *game_state;

			// 새 라운드 시작 
			if (game_act->round == 1) {

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
			}

			// 부모 프로세스 

			close(fd[1]);

			SupportCard get_card[MAX_CARD];
			Dice get_dice;

			read(fd[0], &get_dice, sizeof(get_dice));
			read(fd[0], &get_card, sizeof(get_card));
			close(fd[0]);

			printf("흰 주사위: %d\n흑 주사위: %d\n\n", get_dice.white_dice, get_dice.black_dice);
			printf("-사용 가능한 서포트 카드\n");
			for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
			{
				printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);
			}
			printf("\n");

			while (1) {

				printf("1. 카드 선택\n2. 공격\n3. 턴 종료\ninput: ");
				scanf("%d", &act);
				printf("\n");

				if (act == 1) {

					printf("\n");

					printf("-사용 가능한 서포트 카드\n");

					for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
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

						printf("%s 카드를 사용했습니다.\n\n", get_card[act - 1].name);

						if (get_card[act - 1].white_cost == 0) signo = SIGFPE;
						else if (get_card[act - 1].white_cost == 1) signo = SIGTSTP;
						else if (get_card[act - 1].white_cost == 3) signo = SIGALRM;

						break;
					}

				}

				else if (act == 2) {

					printf("사용할 주사위를 선택하세요.\n1. 백\n2. 흑\ninput: ");
					scanf("%d", &act);

					if ((act == 1 && get_dice.white_dice > 0) || (act == 2 && get_dice.black_dice > 0)) {

						if (act == 1) get_dice.white_dice -= 1;
						else if (act == 2) get_dice.black_dice -= 1;

						int dmg = 5 + temp_state.cyno_dmg - temp_state.tighnari_def;

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
					signo = SIGUSR2;
					break;

				}

			}

			printf("\nwaiting. . .\n\n");
			kill(cy_pid, signo);
			pause();
			

		}
	}

	

	return 0;
}

