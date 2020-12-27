
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define MAGIC_1			(0x1f)  	
#define MAGIC_2			(0x9D)   
#define FIRST			(257)		
#define BIT_MASK        (0x1f)   
#define BLOCK_MODE      (0x80)   

#define BITS 12                   /* Установка количетсва бит на 12 */
#define HASHING_SHIFT (BITS-8)   
#define MAX_VALUE (1 << BITS) - 1 
#define MAX_CODE MAX_VALUE - 1    

#if BITS == 14
#define TABLE_SIZE 18041        /* Размер таблицы должен быть простым числом */
#endif                          /* которые немного больше чем 2 бит			 */
#if BITS == 13                  
#define TABLE_SIZE 9029
#endif
#if BITS <= 12
#define TABLE_SIZE 5021
#endif

void *malloc();

int *code_value;                  /* Массив значений кода                   */
unsigned int *prefix_code;        /* Массив содержащий коды префиксов       */
unsigned char *append_character;  /* Массив содержащий кдобавленные символы */
unsigned char decode_stack[4000]; /* Массив содержащий декодированную строку*/

// Тут всё понятно
void compress(FILE *input, FILE *output);
void expand(FILE *input, FILE *output);
int find_match(int hash_prefix, unsigned int hash_character);
void output_code(FILE *output, unsigned int code, const int n_bits);
unsigned int input_code(FILE *input, const int n_bits);
unsigned char *decode_string(unsigned char *buffer, unsigned int code);

/********************************************************************
**
** Эта программа получает имя файла из командной строки.  Она сжимает файл и помещает 
** его в файл с названием test.lzw.  Затем она разжимает сжатый файл
** test.lzw в test.out.  Test.out должен быть точной копией исходного файла если ему сменить расширение.
**
*************************************************************************/

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "Russian");
	FILE *input_file;
	FILE *output_file;
	FILE *lzw_file;
	char input_file_name[256];

	/*
	**  Для сжатия необходимы три буфера.
	*/
	code_value = (int*)malloc(TABLE_SIZE * sizeof(int));
	prefix_code = (unsigned int *)malloc(TABLE_SIZE * sizeof(unsigned int));
	append_character = (unsigned char *)malloc(TABLE_SIZE * sizeof(unsigned char));
	if (code_value == NULL || prefix_code == NULL || append_character == NULL)
	{
		printf("Критическая ошибка при выделении табличного пространства!\n");
		exit(-1);
	}
	/*
	** Получаем путь/имя файла, открытие, открытие lzw файла.
	*/
	if (argc > 1)
		strcpy(input_file_name, argv[1]);
	else
	{
		printf("Введите полный адрес файла: ");
		scanf("%s", input_file_name);
	}
	input_file = fopen(input_file_name, "rb");
	lzw_file = fopen("test.lzw", "wb");
	if (input_file == NULL || lzw_file == NULL)
	{
		printf("Критическая ошибка при открытии файла.\n");
		exit(-1);
	};
	/*
	** Сжатие файла.
	*/
	compress(input_file, lzw_file);
	fclose(input_file);
	fclose(lzw_file);
	free(code_value);
	/*
	** Открытие фнеобходимых файлов.
	*/
	lzw_file = fopen("test.lzw", "rb");
	output_file = fopen("test.out", "wb");
	if (lzw_file == NULL || output_file == NULL)
	{
		printf("Критическая ошибка при открытии файла.\n");
		exit(-2);
	};
	/*
	** Развёртывание файла.
	*/
	expand(lzw_file, output_file);
	fclose(lzw_file);
	fclose(output_file);

	free(prefix_code);
	free(append_character);
}



/*
** Процедура сжатия.
*/
void compress(FILE *input, FILE *output)
{
	unsigned int next_code;
	unsigned int character;
	unsigned int string_code;
	unsigned int index;
	int i;

	next_code = FIRST;              /* Next_code это следуйщий доступный строковый код*/
	for (i = 0; i < TABLE_SIZE; i++)  /* Очистка таблицы перед началом */
		code_value[i] = -1;

	/* Вывод заголовка */
	fputc(MAGIC_1, output);
	fputc(MAGIC_2, output);
	fputc(BITS | BLOCK_MODE, output);

	i = 0;
	printf("Сжатие...\n");
	string_code = getc(input);    /* Получение первого кода */

  /*
  ** НАчало основного цикла. Этот цикл запускается если все входные данные исчерпаны.
  */
	while ((character = getc(input)) != (unsigned)EOF)
	{
		if (++i == 1000)      // "загрузка"            
		{                                      
			i = 0;                                   
			printf("*");
		}
		index = find_match(string_code, character); /* Посмотреть находится ли строка в  */
		if (code_value[index] != -1)                /* таблице.  Если да,                */
			string_code = code_value[index];        /* то получаем значение.             */
		else                                        /* Если строка не находится в таблице*/
		{                                           /* попытка добавить это              */
			if (next_code <= MAX_CODE)
			{
				code_value[index] = next_code++;
				prefix_code[index] = string_code;
				append_character[index] = character;
			}
			output_code(output, string_code, BITS);  /* Если строка была найдёна */
			string_code = character; 
		}                                   /* Вывод последней строки */
	}                                     /* после добавление новой */
  /*
  ** Конец главного цикла.
  */

	output_code(output, string_code, BITS); /* Вывод последнего кода */
	output_code(output, 0, BITS);           /* Очистка выходного буфера */
	printf("\n");
}


/*
** Процедура хеширования. Выполняется поиск совпадений для префикса +char
** в таблице строк. Если он его находит, возвращается индекс.  Если
** строка всё же не была найдена, вместо неё возвращается первый доступный индекс в таблице.
*/
int find_match(int hash_prefix, unsigned int hash_character)
{
	int index;
	int offset;

	index = (hash_character << HASHING_SHIFT) ^ hash_prefix;
	if (index == 0)
		offset = 1;
	else
		offset = TABLE_SIZE - index;
	while (1)
	{
		if (code_value[index] == -1)
			return(index);
		if (prefix_code[index] == hash_prefix &&
			append_character[index] == hash_character)
			return(index);
		index -= offset;
		if (index < 0)
			index += TABLE_SIZE;
	}
}


/*
**  Процедура "расширения"/"развёртывания".  Он берёт LZW файли расжимает его в out файл
*/
void expand(FILE *input, FILE *output)
{
	unsigned int next_code;
	unsigned int new_code;
	unsigned int old_code;
	int character;
	int counter;
	unsigned char *string;
	int		n_bits;
	int		block_mode;

	if (fgetc(input) != MAGIC_1) return;
	if (fgetc(input) != MAGIC_2) return;
	character = fgetc(input);
	n_bits = character & BIT_MASK;
	block_mode = character & BLOCK_MODE;

	next_code = FIRST;           /* Следуйщий код для определения */
	counter = 0;               
	printf("Развёртывание...\n");

	old_code = input_code(input, n_bits);  
	character = old_code;         
	putc(old_code, output);      
  /*
  **  НАчало основного цикла.  Он считывает символы из файла LZW до тех пор, 
  **  пока не увидит спец код, который используется для обозначения конца данных
  */
	while ((new_code = input_code(input, n_bits)) != (MAX_VALUE))
	{
		if (++counter == 1000)   
		{                     
			counter = 0;         
			printf("*");
		}
		/*
		** Этот код проверяет наличие специального символа STRING + CHARACTER + STRING + CHARACTER + STRING.
		** который генерирует неопределенный код.  Он обрабатывает это, 
		** декодируя последний код и добавляя один символ в конец строки декодирования.
		*/
		if (new_code >= next_code)
		{
			*decode_stack = character;
			string = decode_string(decode_stack + 1, old_code);
		}
		/*
		** В противном случае мы делаем прямое декодирование нового кода.
		*/
		else
			string = decode_string(decode_stack, new_code);
		/*
		** Теперь выводим декодированную строку в обратном порядке.
		*/
		character = *string;
		while (string >= decode_stack)
			putc(*string--, output);
		/*
		** Добавляем новый код в таблицу строк
		*/
		if (next_code <= MAX_CODE)
		{
			prefix_code[next_code] = old_code;
			append_character[next_code] = character;
			next_code++;
		}
		old_code = new_code;
	}
	printf("\n");
}

/*
** Эта процедура просто декодирует строку из таблицы строк и сохраняет ее в буфере. 
** Затем буфер может быть выведен программой расширения в обратном порядке.
*/

unsigned char *decode_string(unsigned char *buffer, unsigned int code)
{
	int i;

	i = 0;
	while (code > 255)
	{
		*buffer++ = append_character[code];
		code = prefix_code[code];
		if (i++ >= MAX_CODE)
		{
			printf("Критическая ошибка при расширении кода.\n");
			exit(-3);
		}
	}
	*buffer = code;
	return(buffer);
}

/*
** Следующие две процедуры используются для вывода кодов переменной длины.
*/

unsigned int input_code_ORIG(FILE *input)
{
	unsigned int return_value;
	static int input_bit_count = 0;
	static unsigned long input_bit_buffer = 0L;

	while (input_bit_count <= 24)
	{
		input_bit_buffer |=
			(unsigned long)getc(input) << (24 - input_bit_count);
		input_bit_count += 8;
	}
	return_value = input_bit_buffer >> (32 - BITS);
	input_bit_buffer <<= BITS;
	input_bit_count -= BITS;
	return(return_value);
}


inline unsigned int mask(const int n_bits)
{
	static unsigned int		mask = 0;
	static int					n_bits_prev = 0;

	if (n_bits_prev == n_bits) return mask;
	n_bits_prev = n_bits;

	mask = 0;
	for (int i = 0; i < n_bits; i++)
	{
		mask <<= 1;
		mask |= 1;
	}

	return mask;
}


static unsigned int input_code(FILE *input, const int n_bits)
{
	int						c;
	unsigned int			return_value;
	static int				input_bit_count = 0;
	static unsigned long input_bit_buffer = 0L;

	while (input_bit_count < n_bits)
	{
		if ((c = getc(input)) == EOF) return MAX_VALUE;
		input_bit_buffer |= c << input_bit_count;
		input_bit_count += 8;
	}

	return_value = input_bit_buffer & mask(n_bits);
	input_bit_buffer >>= n_bits;
	input_bit_count -= n_bits;
	return return_value;
}

void output_code_ORIG(FILE *output, unsigned int code)
{
	static int output_bit_count = 0;
	static unsigned long output_bit_buffer = 0L;

	output_bit_buffer |= (unsigned long)code << (32 - BITS - output_bit_count);
	output_bit_count += BITS;
	while (output_bit_count >= 8)
	{
		putc(output_bit_buffer >> 24, output);
		output_bit_buffer <<= 8;
		output_bit_count -= 8;
	}
}

static void output_code(FILE *output, unsigned int code, const int n_bits)
{
	static int output_bit_count = 0;
	static unsigned long output_bit_buffer = 0L;

	output_bit_buffer |= (unsigned long)code << output_bit_count;
	output_bit_count += n_bits;

	while (output_bit_count >= 8)
	{
		putc(output_bit_buffer & 0xff, output);
		output_bit_buffer >>= 8;
		output_bit_count -= 8;
	}
}