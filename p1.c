#include <arch/zx.h>
#include <input.h>
#include <stdlib.h>
#include <z80.h>
#include <input/input_zx.h>
#include <stdio.h>
#include <string.h>


// snake segment
struct Seg
{
	unsigned char x;
	unsigned char y;
	signed char dir_x;
	signed char dir_y;
};

// negative or positive items
#define ITEM_FREE 0
#define ITEM_NEGATIVE 1
#define ITEM_POSITIVE 2
struct Item
{
	char type;
	char x;
	char y; // coords
};

typedef char* (*Tattr_addr) (char x, char y);
Tattr_addr attr_addr;

#define max_snake 128
#define max_items 10

unsigned char snake_size = 1;
char* ico_snake = NULL;
char* atr_snake = NULL;

const char ico_peach[] = {
	0b00000100,
	0b00001000,
	0b00011000,
	0b00011000,
	0b00111100,
	0b01111110,
	0b00111100,
	0b00011000
};

const char attr_peach[] = {
	0b00111100,
	0b00111100,
	0b00111110,
	0b00111110,
	0b00111110,
	0b00111110,
	0b00111110,
	0b00111110
};
const char attr_peach_shift = 6; // zx spectrum attr clash point

const char ico_mush[] = {
	0b00010000,
	0b00101000,
	0b01010100,
	0b10101010,
	0b10101010,
	0b00010000,
	0b00010000,	
	0b00010000,	
};
const char attr_mush[] = {
	0b00111010,
	0b00111010,
	0b00111010,
	0b00111010,
	0b00111010,
	0b00111000,
	0b00111000,	
	0b00111000
};

const char attr_mush_shift = 3; // zx spectrum attr clash point

struct Seg snake[max_snake];
struct Item items[max_items];

__sfr __at 0xFF IOFF; 

const unsigned char min_x = 5;
const unsigned char min_y = 5;
const unsigned char max_x = 240;
const unsigned char max_y = 187;

char mush_x = 0;
char mush_y = 0;
	
void plot(unsigned char x, unsigned char y)
{
	*zx_pxy2saddr(x,y) |= zx_px2bitmask(x);
}

void clear(unsigned char x, unsigned char y)
{
	*zx_pxy2saddr(x,y) &= ~zx_px2bitmask(x);
}

char* attr_addr_zx (char x, char y)
{
	return zx_pxy2aaddr(x,y);
}

char* attr_addr_timex (char x, char y)
{
	// 8 kb more than screen
	return zx_pxy2saddr(x,y) + 0x2000;
}

void PrintHead(unsigned char x, unsigned char y, signed char dir_x, signed char dir_y)
{
	char ico_start = 0; // for up
	if (dir_y > 0)
	{
		ico_start = 8;
		// cleanup line above top
		*zx_pxy2saddr(x, y-1) = 0;
		if (x%8 != 0)
			*zx_pxy2saddr(x+8, y-1) = 0;
	}
	else if (dir_x < 0)
	{
		ico_start = 16;
		//clenup next column
		if (x%8==0)
			for (char row = 0; row < 8; row++)
				*zx_pxy2saddr(x+8, y+row) &= 0x7F;	
	}
	else if (dir_x > 0)
	{
		ico_start = 24;
		//clenup prev column
		if (x%8==0)
			for (char row = 0; row < 8; row++)
				*zx_pxy2saddr(x-dir_x, y+row) &= 0xFE;	
	}
	else if (dir_y < 0)
	{
		// cleanup line below bottom
		*zx_pxy2saddr(x, y+8) = 0;
		if (x%8 != 0)
			*zx_pxy2saddr(x+8, y+8) = 0;
	}
	
	for (unsigned char row = 0; row < 8; row++)
	{
		char shiftL = x & 0x07;
		char vL = ico_snake[ico_start+row] >> shiftL;
		*zx_pxy2saddr(x, y+row) = vL;
		*attr_addr(x, y+row) = atr_snake[ico_start+row];
		if (shiftL != 0)
		{
			char shiftR = 8-shiftL;
			char vH = ico_snake[ico_start+row] << shiftR;
			*zx_pxy2saddr(x+8, y+row) = vH;
			*attr_addr(x+8, y+row) = atr_snake[ico_start+row];
		}
	}
}

void PrintFull(unsigned char x, unsigned char y,
	signed char dir_x, signed char dir_y)
{
	if (dir_y > 0)
	{
		// cleanup line below bottom
		*zx_pxy2saddr(x, y-1) = 0;
		if (x%8 != 0)
			*zx_pxy2saddr(x+8, y-1) = 0;
	}
	else if (dir_y < 0)
	{
		// cleanup line above top
		*zx_pxy2saddr(x, y+8) = 0;
		if (x%8 != 0)
			*zx_pxy2saddr(x+8, y+8) = 0;
	}
		
	char ico_start = 32;
	for (char row = 0; row < 8; row++)
	{
		char shiftL = x & 0x07;
		char maskL = ~(0xFF >> shiftL);
		if (shiftL != 0 && dir_x > 0)
			maskL = ~(0xFF >> (shiftL-1));
		char vL = ico_snake[ico_start+row] >> shiftL;
		*zx_pxy2saddr(x, y+row) = (*zx_pxy2saddr(x, y+row)& maskL) | vL;
		// allow little attribute clash if moving -x
		if (dir_x >= 0 || shiftL < 6)  
			*attr_addr(x, y+row) = atr_snake[ico_start+row];
		if (shiftL != 0)
		{
			char shiftR = 8-shiftL;
			char maskR = ~(0xFF << shiftR);
			if (dir_x < 0)
				maskR = ~(0xFF << shiftR-1);
			char vH = ico_snake[ico_start+row] << shiftR;
			*zx_pxy2saddr(x+8, y+row) 
				= (*zx_pxy2saddr(x+8, y+row)& maskR) | vH;
			// allow little attribute clash if movin +x
			if (dir_x <= 0 || shiftL > 3)  
				*attr_addr(x+8, y+row) = atr_snake[ico_start+row];
		}
		else if (dir_x > 0)
		{
			// moved from byte on left, so empty the last bit
			*zx_pxy2saddr(x-8, y+row) &= 0xFE;
		}
		else if (dir_x < 0)
		{
			// moved from byte on right, so empty the first bit
			*zx_pxy2saddr(x+8, y+row) &= 0x7F;
		}
	}
}

void PrintIco(const char* ico, const char* attr,
	unsigned char x, unsigned char y)
{
	for (int row = 7; row >= 0; row--)
	{
		*zx_pxy2saddr(x, y+row) = ico[row];
		*attr_addr(x, y+row) = attr[row];
	}
}

void ClearItem(unsigned char x, unsigned char y)
{
	for (int row = 7; row >= 0; row--)
	{
		*zx_pxy2saddr(x, y+row) = 0x00;
		*attr_addr(x, y+row) = 0b00111000;
	}
}
void PrepareIcoSnake()
{
	ico_snake = calloc(40,1); // 5 versions
	atr_snake = calloc(40,1); // 5 versions
	ico_snake[0] = 0b00011000; // up
	ico_snake[1] = 0b00111100;
	ico_snake[2] = 0b01011010;
	ico_snake[3] = 0b11111111;
	ico_snake[4] = 0b11111111;
	ico_snake[5] = 0b01100110;
	ico_snake[6] = 0b00111100;	
	ico_snake[7] = 0b00011000;	

	ico_snake[15] = 0b00011000; // down
	ico_snake[14] = 0b00111100;
	ico_snake[13] = 0b01011010;
	ico_snake[12] = 0b11111111;
	ico_snake[11] = 0b11111111;
	ico_snake[10] = 0b01100110;
	ico_snake[9]  = 0b00111100;	
	ico_snake[8]  = 0b00011000;

	ico_snake[16] = 0b00011000; // left
	ico_snake[17] = 0b00111100;
	ico_snake[18] = 0b01011110;
	ico_snake[19] = 0b11111111;
	ico_snake[20] = 0b11111111;
	ico_snake[21] = 0b00011110;
	ico_snake[22] = 0b00111100;	
	ico_snake[23] = 0b00011000;
	
	ico_snake[24] = 0b00011000; // right
	ico_snake[25] = 0b00111100;
	ico_snake[26] = 0b01111010;
	ico_snake[27] = 0b11111111;
	ico_snake[28] = 0b11111111;
	ico_snake[29] = 0b01111000;
	ico_snake[30] = 0b00111100;	
	ico_snake[31] = 0b00011000;	
	
	ico_snake[32] = 0b00011000; // full
	ico_snake[33] = 0b00111100;
	ico_snake[34] = 0b01111110;
	ico_snake[35] = 0b11111111;
	ico_snake[36] = 0b11111111;
	ico_snake[37] = 0b01111110;
	ico_snake[38] = 0b00111100;	
	ico_snake[39] = 0b00011000;	

	atr_snake[0] = 0b00111010; // up
	atr_snake[1] = 0b00111000;
	atr_snake[2] = 0b00111000;
	atr_snake[3] = 0b00111000;
	atr_snake[4] = 0b00111000;
	atr_snake[5] = 0b00111000;
	atr_snake[6] = 0b00111000;	
	atr_snake[7] = 0b00111000;	

	atr_snake[15] = 0b00111010; // down
	atr_snake[14] = 0b00111000;
	atr_snake[13] = 0b00111000;
	atr_snake[12] = 0b00111000;
	atr_snake[11] = 0b00111000;
	atr_snake[10] = 0b00111000;
	atr_snake[9]  = 0b00111000;	
	atr_snake[8]  = 0b00111000;

	atr_snake[16] = 0b00111010; // left
	atr_snake[17] = 0b00111000;
	atr_snake[18] = 0b00111000;
	atr_snake[19] = 0b00111000;
	atr_snake[20] = 0b00111000;
	atr_snake[21] = 0b00111100;
	atr_snake[22] = 0b00111000;	
	atr_snake[23] = 0b00111000;
	
	atr_snake[24] = 0b00111010; // right
	atr_snake[25] = 0b00111000;
	atr_snake[26] = 0b00111000;
	atr_snake[27] = 0b00111000;
	atr_snake[28] = 0b00111000;
	atr_snake[29] = 0b00111100;
	atr_snake[30] = 0b00111000;	
	atr_snake[31] = 0b00111000;	
	
	atr_snake[32] = 0b00111000; // full
	atr_snake[33] = 0b00111000;
	atr_snake[34] = 0b00111000;
	atr_snake[35] = 0b00111000;
	atr_snake[36] = 0b00111000;
	atr_snake[37] = 0b00111000;
	atr_snake[38] = 0b00111000;	
	atr_snake[39] = 0b00111000;	


}

void Grow()
{
	if (snake_size + 8 >= max_snake)
		return;
	for( char i=0; i<8; i++)
	{
		snake_size++;
		// preserve last element direction in new entry
		char curr = snake_size-1;
		char prev = curr-1;
		snake [curr] = snake[prev];
		// advance coord in oposite direction
		snake [curr].x -= snake [prev].dir_x;
		snake [curr].y -= snake [prev].dir_y;
	}
}

int GetColisionWithItem (char nx, char ny)
{
	for (char i = 0; i < max_items; i++)
	{
		if (items[i].type != ITEM_FREE &&
		(
			(
			nx >= items[i].x &&
			nx <= items[i].x + 7 &&
			ny >= items[i].y &&
			ny <= items[i].y + 7
			)
			||
			(
			items[i].x >= nx &&
			items[i].x <= nx + 7 &&
			items[i].y >= ny &&
			items[i].y <= ny + 7
			)
		))
		{
			return i;
		}
	}
	return -1;
}

// 1 -> negative
// 2 -> positive
void CreateItem(char item_type)
{
	signed char index = -1;
	
	// find free entry
	for (char i = 0; i < max_items; i++)
	{
		if (items[i].type == ITEM_FREE)
		{
			index = i;
			break;
		}
	}
	if (index >= 0)
	{
		char nx = (rand()%(max_x-min_x) + min_x) & 0xF8;

		// zx spectrum attribute clash reduction
		char attr_y_shift;
		if (ITEM_POSITIVE == item_type)
			attr_y_shift = attr_peach_shift;
		else
			attr_y_shift = attr_mush_shift;
		char ny = (rand()%(max_y-min_y) + min_y) & 0xF8 | attr_y_shift;
		while (GetColisionWithItem(nx, ny) != -1)
		{
			nx = (rand()%(max_x-min_x) + min_x)  & 0xF8;
			ny = (rand()%(max_y-min_y) + min_y) & 0xF8 | attr_y_shift;
		}
		items[index].type = item_type;
		items[index].x = nx;		
		items[index].y = ny;		

		if (ITEM_POSITIVE == item_type)
			PrintIco(ico_peach, attr_peach, nx, ny);
		else
			PrintIco(ico_mush, attr_mush, nx, ny);
	}
}
		
int Move(char* substep)
{
	for (int i = snake_size-1; i>=0; i--)
	{
		if (i>0)
		{
			snake[i] = snake[i-1];
		}
		else
		{
			snake [i].x += snake[i].dir_x;
			snake [i].y += snake[i].dir_y;
			if (snake[0].x > max_x || 
				snake[0].x < min_x ||
				snake[0].y > max_y ||
				snake[0].y < min_y)
			{
				return -1;
			}

			int col_item = GetColisionWithItem(snake[i].x, snake[i].y);
			if (col_item != -1)
			{

				if (ITEM_POSITIVE == items[col_item].type)
				{
					items[col_item].type = ITEM_FREE;
					ClearItem(items[col_item].x, items[col_item].y);
					Grow();
				}
				else
				{
					//printf ("(%d)x: %d %d, y: %d %d ",
						//items[col_item].type,
						//snake[i].x, items[col_item].x,
						//snake[i].y, items[col_item].y);
					return -1;
				}
			}			
		}
	}
	
	// we grow
	if (*substep >> 4 == 1) // modulo 16
	{
		*substep = 0;
		if (rand() & 0x01 == 0x01)
			CreateItem(ITEM_POSITIVE);
		else
			CreateItem(ITEM_NEGATIVE);
	}	

	return 0;
}

void GameLoop()
{
	char substep = 0;
	char go = 1;
	
	while (go)
	{
		// move a snake on direction
		// apply all changes in an array, but only plot first and clear last
		// 1 calculate pos after moving
		if (Move(&substep) != 0)
		{
			go = 0;
			continue;
		}
		// draw at new position
		for (int i = 0; i < snake_size; i+=8)
		{
			if (i != 0)
			{
				PrintFull (snake[i].x, snake[i].y, snake[i].dir_x, snake[i].dir_y);
			}
			else
			//if (0 == i)
			{
				PrintHead (snake[0].x, snake[0].y, snake[0].dir_x, snake[0].dir_y);
			}
		}
		substep++;

		// check moving direction
		z80_delay_ms(100);
		switch (in_stick_kempston())
		{
			case IN_STICK_UP:
				if (snake[0].dir_y != -1)
				{
					snake[0].dir_x = 0;
					snake[0].dir_y = -1;
					//substep = 0;
				}
				break;
			case IN_STICK_DOWN:
				if (snake[0].dir_y != 1)
				{
					snake[0].dir_x = 0;
					snake[0].dir_y = 1;
					//substep = 0;
				}
				break;
			case IN_STICK_LEFT:
				if (snake[0].dir_x != -1)
				{
					snake[0].dir_x = -1;
					snake[0].dir_y = 0;
					//substep = 0;
				}
				break;
			case IN_STICK_RIGHT:
				if (snake[0].dir_x != 1)
				{
					snake[0].dir_x = 1;
					snake[0].dir_y = 0;
					//substep = 0;
				}
				break;
		}
	}
}

int main(void)
{
	struct Seg p_start;
	
	p_start.x = 120;
	p_start.y = 90;
	
	p_start.dir_x = 0;
	p_start.dir_y = -1;
		
	attr_addr = attr_addr_zx; // detect timex
	IOFF = 2;
	if (IOFF == 2)
	{
		attr_addr = attr_addr_timex;
	}
	
	PrepareIcoSnake();
	
	while (1)
	{
		snake_size = 1;
		snake[0] = p_start;
		if (IOFF == 2)
		{
			memset((void*)0x6000, 0b00111000, 6144);
		}
		zx_cls(PAPER_WHITE);
		// clear items
		for (char i = 0; i < max_items; i++)
		{
			items[i].type = ITEM_FREE;
		}

		GameLoop();
		while (in_stick_kempston() != IN_STICK_FIRE)
			;
	}
//	free(ico_snake);
//	return 0;
}
  