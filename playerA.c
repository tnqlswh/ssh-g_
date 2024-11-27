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

int main()
{	
	int round = 1; // 첫 라운드인지 확인용 변수
	int act = 0; // 플레이어 행동 입력 받기용 변수 

	// 부모 프로세스랑 자식 프로세스 분리해둔 거 안에 있어서 밖에서 사용이 안 되길래 빼왔음
	SupportCard get_card[MAX_CARD];
	Dice get_dice;

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

	int fd[2];
	pid_t cardDice;

	pipe(fd);
	cardDice = fork();
	if (game_state->current_turn == 0) {
		
		if (round == 1) {
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
				
				read(fd[0], &get_dice, sizeof(get_dice));
				read(fd[0], &get_card, sizeof(get_card));
				close(fd[0]);

				printf("흰 주사위: %d\n흑 주사위: %d\n", get_dice.white_dice, get_dice.black_dice);
				printf("-사용 가능한 서포트 카드\n");
				for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
				{
					printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);
				}
			}
		}

		while (act != 3) {

			printf("1. 카드 선택\n2. 공격\n3. 턴 종료\ninput: ");
			scanf("%d", &act);

			if (act == 1) {

				printf("-사용 가능한 서포트 카드\n");
				
				// 제대로 출력 안 되는 오류 있음. 이거 자식 프로세스임? 
				for (int i = 0; i < sizeof(get_card) / sizeof(get_card[0]); i++)
				{
					printf("%d. %s\n소모 주사위\n-흑: %d\n-백: %d\n", i + 1, get_card[i].name, get_card[i].black_cost, get_card[i].white_cost);

				}

				printf("input: ");
				scanf("%d", &act);

				if (get_dice.white_dice < get_card[act-1].white_cost || get_dice.black_dice < get_card[act-1].black_cost)
					printf("주사위가 부족합니다.\n");

				act = 0;

			}

			else if (act == 2) {

				printf("상대를 때립니다. 아야~");

				act = 3;

			}

		}
		
		
	}
	

	return 0;
}
