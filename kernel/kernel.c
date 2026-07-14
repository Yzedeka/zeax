#include <stdint.h>

#define VGA_MEMORY 0xB8000

#define SECTOR_SIZE 512

#define MAX_FILES 32
#define MAX_FILENAME 32
#define MAX_USERNAME 32
#define MAX_PASSWORD 32

#define SCREEN_COLS 80
#define SCREEN_ROWS 25
#define SCROLLBACK_LINES 200


#define COLOR_BLACK         0x0
#define COLOR_BLUE          0x1
#define COLOR_GREEN         0x2
#define COLOR_CYAN          0x3
#define COLOR_RED           0x4
#define COLOR_MAGENTA       0x5
#define COLOR_BROWN         0x6
#define COLOR_LIGHT_GREY    0x7
#define COLOR_DARK_GREY     0x8
#define COLOR_LIGHT_BLUE    0x9
#define COLOR_LIGHT_GREEN   0xA
#define COLOR_LIGHT_CYAN    0xB
#define COLOR_LIGHT_RED     0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW        0xE
#define COLOR_WHITE         0xF


volatile uint16_t* vga = (uint16_t*)VGA_MEMORY;

int cursor = 0;

uint8_t current_color = COLOR_LIGHT_GREY;

static int shift_held = 0;

int disk_ok = 0;
int needs_signup = 0;

char username[MAX_USERNAME] = "user";
char password[MAX_PASSWORD] = "";




#define FS_MAGIC 0x5A454158

#define SB_LBA 0
#define FT_LBA_START 1
#define FT_SECTORS 3
#define FILE_DATA_LBA_START (FT_LBA_START + FT_SECTORS)


typedef struct __attribute__((packed))
{
    uint32_t magic;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
} superblock_t;


typedef struct __attribute__((packed))
{
    char name[MAX_FILENAME];
    uint16_t size;
    uint8_t used;
} file_entry_t;


file_entry_t file_table[MAX_FILES];



static inline unsigned char inb(unsigned short port)
{
    unsigned char ret;

    asm volatile(
        "inb %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );

    return ret;
}



static inline void outb(unsigned short port, unsigned char value)
{
    asm volatile(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}



static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;

    asm volatile(
        "inw %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );

    return ret;
}



static inline void outw(uint16_t port, uint16_t value)
{
    asm volatile(
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}



void update_cursor()
{
    unsigned short pos = cursor;

    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);

    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}



uint16_t scrollback[SCROLLBACK_LINES][SCREEN_COLS];
int scrollback_count = 0;
int scrollback_next = 0;
uint16_t live_snapshot[SCREEN_ROWS][SCREEN_COLS];
int in_scrollback = 0;
int view_offset = 0;


void scrollback_push(volatile uint16_t* line)
{
    for(int i = 0; i < SCREEN_COLS; i++)
        scrollback[scrollback_next][i] = line[i];

    scrollback_next = (scrollback_next + 1) % SCROLLBACK_LINES;

    if(scrollback_count < SCROLLBACK_LINES)
        scrollback_count++;
}


void get_history_line(int index, uint16_t* out)
{
    if(index < scrollback_count)
    {
        int start = (scrollback_count < SCROLLBACK_LINES) ? 0 : scrollback_next;
        int real_index = (start + index) % SCROLLBACK_LINES;

        for(int c = 0; c < SCREEN_COLS; c++)
            out[c] = scrollback[real_index][c];
    }
    else
    {
        int snap_row = index - scrollback_count;

        for(int c = 0; c < SCREEN_COLS; c++)
            out[c] = live_snapshot[snap_row][c];
    }
}


void render_scrollback()
{
    int total = scrollback_count + SCREEN_ROWS;
    int top_line = total - SCREEN_ROWS - view_offset;

    if(top_line < 0)
        top_line = 0;

    uint16_t line_buf[SCREEN_COLS];

    for(int r = 0; r < SCREEN_ROWS; r++)
    {
        get_history_line(top_line + r, line_buf);

        for(int c = 0; c < SCREEN_COLS; c++)
            vga[r * SCREEN_COLS + c] = line_buf[c];
    }
}


void enter_scrollback()
{
    if(in_scrollback)
        return;

    for(int r = 0; r < SCREEN_ROWS; r++)
        for(int c = 0; c < SCREEN_COLS; c++)
            live_snapshot[r][c] = vga[r * SCREEN_COLS + c];

    in_scrollback = 1;
}


void exit_scrollback()
{
    if(!in_scrollback)
        return;

    for(int r = 0; r < SCREEN_ROWS; r++)
        for(int c = 0; c < SCREEN_COLS; c++)
            vga[r * SCREEN_COLS + c] = live_snapshot[r][c];

    in_scrollback = 0;
    view_offset = 0;

    update_cursor();
}


void scroll()
{
    scrollback_push(&vga[0]);

    for(int i = 0; i < 80 * 24; i++)
    {
        vga[i] = vga[i + 80];
    }

    for(int i = 80 * 24; i < 80 * 25; i++)
    {
        vga[i] = ' ' | ((uint16_t)current_color << 8);
    }

    cursor -= 80;
}



void set_color(uint8_t color)
{
    current_color = color;
}



void putchar(char c)
{
    if(c == '\n')
    {
        cursor += 80 - (cursor % 80);
    }
    else
    {
        vga[cursor] = (uint16_t)c | ((uint16_t)current_color << 8);
        cursor++;
    }

    if(cursor >= 80 * 25)
    {
        scroll();
    }

    update_cursor();
}



void print(const char* str)
{
    while(*str)
    {
        putchar(*str);
        str++;
    }
}



void print_color(const char* str, uint8_t color)
{
    uint8_t prev = current_color;

    set_color(color);
    print(str);
    set_color(prev);
}



void clear()
{
    for(int i = 0; i < 80 * 25; i++)
    {
        vga[i] = ' ' | ((uint16_t)current_color << 8);
    }

    cursor = 0;

    update_cursor();
}



void print_number(uint32_t n)
{
    char buffer[16];
    int i = 0;


    if(n == 0)
    {
        putchar('0');
        return;
    }


    while(n > 0)
    {
        buffer[i++] = '0' + (n % 10);
        n /= 10;
    }


    while(i--)
    {
        putchar(buffer[i]);
    }
}



static int extended_prefix = 0;

char keyboard()
{
    if(!(inb(0x64) & 1))
        return 0;


    unsigned char sc = inb(0x60);


    if(sc == 0xE0)
    {
        extended_prefix = 1;
        return 0;
    }

    if(extended_prefix)
    {
        extended_prefix = 0;

        if(sc & 0x80)
            return 0;

        if(sc == 0x49)
        {
            enter_scrollback();

            if(view_offset < scrollback_count)
                view_offset++;

            render_scrollback();
            return 0;
        }

        if(sc == 0x51)
        {
            if(in_scrollback)
            {
                if(view_offset > 0)
                    view_offset--;

                if(view_offset == 0)
                    exit_scrollback();
                else
                    render_scrollback();
            }

            return 0;
        }

        return 0;
    }


    if(sc & 0x80)
    {
        unsigned char released = sc & 0x7F;

        if(released == 0x2A || released == 0x36)
            shift_held = 0;

        return 0;
    }


    if(sc == 0x2A || sc == 0x36)
    {
        shift_held = 1;
        return 0;
    }


    if(in_scrollback)
        exit_scrollback();


    switch(sc)
    {
        case 0x10:return shift_held ? 'Q' : 'q';
        case 0x11:return shift_held ? 'W' : 'w';
        case 0x12:return shift_held ? 'E' : 'e';
        case 0x13:return shift_held ? 'R' : 'r';
        case 0x14:return shift_held ? 'T' : 't';
        case 0x15:return shift_held ? 'Y' : 'y';
        case 0x16:return shift_held ? 'U' : 'u';
        case 0x17:return shift_held ? 'I' : 'i';
        case 0x18:return shift_held ? 'O' : 'o';
        case 0x19:return shift_held ? 'P' : 'p';


        case 0x1E:return shift_held ? 'A' : 'a';
        case 0x1F:return shift_held ? 'S' : 's';
        case 0x20:return shift_held ? 'D' : 'd';
        case 0x21:return shift_held ? 'F' : 'f';
        case 0x22:return shift_held ? 'G' : 'g';
        case 0x23:return shift_held ? 'H' : 'h';
        case 0x24:return shift_held ? 'J' : 'j';
        case 0x25:return shift_held ? 'K' : 'k';
        case 0x26:return shift_held ? 'L' : 'l';


        case 0x2C:return shift_held ? 'Z' : 'z';
        case 0x2D:return shift_held ? 'X' : 'x';
        case 0x2E:return shift_held ? 'C' : 'c';
        case 0x2F:return shift_held ? 'V' : 'v';
        case 0x30:return shift_held ? 'B' : 'b';
        case 0x31:return shift_held ? 'N' : 'n';
        case 0x32:return shift_held ? 'M' : 'm';


        case 0x02:return shift_held ? '!' : '1';
        case 0x03:return shift_held ? '@' : '2';
        case 0x04:return shift_held ? '#' : '3';
        case 0x05:return shift_held ? '$' : '4';
        case 0x06:return shift_held ? '%' : '5';
        case 0x07:return shift_held ? '^' : '6';
        case 0x08:return shift_held ? '&' : '7';
        case 0x09:return shift_held ? '*' : '8';
        case 0x0A:return shift_held ? '(' : '9';
        case 0x0B:return shift_held ? ')' : '0';

        case 0x0C:return shift_held ? '_' : '-';
        case 0x34:return shift_held ? '>' : '.';


        case 0x39:return ' ';

        case 0x1C:return '\n';

        case 0x0E:return '\b';

        case 0x01:return 27; //ESC
    }


    return 0;
}




void read_line(char* buf, int max_len, int mask)
{
    int pos = 0;

    while(1)
    {
        char c = keyboard();

        if(c == 0)
            continue;

        if(c == '\n')
            break;

        if(c == '\b')
        {
            if(pos > 0)
            {
                pos--;

                cursor--;
                vga[cursor] = ' ' | ((uint16_t)current_color << 8);
                update_cursor();
            }

            continue;
        }

        if(pos < max_len - 1)
        {
            buf[pos++] = c;
            putchar(mask ? '*' : c);
        }
    }

    buf[pos] = 0;
}



void reboot()
{
    asm volatile(
        "cli\n"
        "mov $0xFE, %al\n"
        "outb %al, $0x64"
    );
}



void shutdown()
{
    outb(0x604, 0x00);
}



int strcmp(char* a, char* b)
{
    while(*a && (*a == *b))
    {
        a++;
        b++;
    }

    return *(unsigned char*)a - *(unsigned char*)b;
}



int str_len(char* s)
{
    int n = 0;

    while(s[n])
        n++;

    return n;
}



void str_copy_n(char* dst, char* src, int max)
{
    int i = 0;

    while(src[i] && i < max - 1)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}



int split_args(char* cmd, char* argv[], int max_args)
{
    int argc = 0;
    int i = 0;

    while(cmd[i] && argc < max_args)
    {
        while(cmd[i] == ' ')
            i++;

        if(!cmd[i])
            break;

        argv[argc++] = &cmd[i];

        while(cmd[i] && cmd[i] != ' ')
            i++;

        if(cmd[i])
        {
            cmd[i] = 0;
            i++;
        }
    }

    return argc;
}




uint32_t get_ram_kb()
{
    outb(0x70, 0x17);
    uint8_t low = inb(0x71);

    outb(0x70, 0x18);
    uint8_t high = inb(0x71);

    uint32_t extended_kb = ((uint32_t)high << 8) | low;

    return extended_kb + 1024;
}




#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_FLUSH   0xE7

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08


int ata_wait_bsy()
{
    uint32_t timeout = 1000000;

    while((inb(ATA_STATUS) & ATA_SR_BSY) && timeout)
        timeout--;

    return timeout ? 0 : -1;
}


int ata_wait_drq()
{
    uint32_t timeout = 1000000;

    while(!(inb(ATA_STATUS) & ATA_SR_DRQ) && timeout)
        timeout--;

    return timeout ? 0 : -1;
}


int ata_read_sector(uint32_t lba, uint8_t* buffer)
{
    if(ata_wait_bsy() != 0)
        return -1;

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_READ);

    if(ata_wait_bsy() != 0)
        return -1;

    if(ata_wait_drq() != 0)
        return -1;

    uint16_t* buf16 = (uint16_t*)buffer;

    for(int i = 0; i < 256; i++)
        buf16[i] = inw(ATA_DATA);

    return 0;
}


int ata_write_sector(uint32_t lba, uint8_t* buffer)
{
    if(ata_wait_bsy() != 0)
        return -1;

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if(ata_wait_bsy() != 0)
        return -1;

    if(ata_wait_drq() != 0)
        return -1;

    uint16_t* buf16 = (uint16_t*)buffer;

    for(int i = 0; i < 256; i++)
        outw(ATA_DATA, buf16[i]);

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy();

    return 0;
}



// ============================ filesystem ============================
// Disk layout:
//   LBA 0          - superblock (magic + username)
//   LBA 1..3       - file table (32 entries)
//   LBA 4..35      - file contents one 512-byte sector per file slot

void fs_write_table()
{
    if(!disk_ok)
        return;

    static uint8_t buf[SECTOR_SIZE * FT_SECTORS];

    file_entry_t* entries = (file_entry_t*)buf;

    for(int i = 0; i < MAX_FILES; i++)
        entries[i] = file_table[i];

    for(int s = 0; s < FT_SECTORS; s++)
        ata_write_sector(FT_LBA_START + s, buf + (s * SECTOR_SIZE));
}


void fs_read_table()
{
    static uint8_t buf[SECTOR_SIZE * FT_SECTORS];

    for(int s = 0; s < FT_SECTORS; s++)
        ata_read_sector(FT_LBA_START + s, buf + (s * SECTOR_SIZE));

    file_entry_t* entries = (file_entry_t*)buf;

    for(int i = 0; i < MAX_FILES; i++)
        file_table[i] = entries[i];
}


void fs_write_superblock()
{
    if(!disk_ok)
        return;

    static uint8_t buf[SECTOR_SIZE];

    for(int i = 0; i < SECTOR_SIZE; i++)
        buf[i] = 0;

    superblock_t* sb = (superblock_t*)buf;

    sb->magic = FS_MAGIC;
    str_copy_n(sb->username, username, MAX_USERNAME);
    str_copy_n(sb->password, password, MAX_PASSWORD);

    ata_write_sector(SB_LBA, buf);
}


void fs_format()
{
    for(int i = 0; i < MAX_FILES; i++)
    {
        file_table[i].used = 0;
        file_table[i].size = 0;
        file_table[i].name[0] = 0;
    }

    fs_write_table();
    fs_write_superblock();
}


void fs_init()
{
    static uint8_t buf[SECTOR_SIZE];

    if(ata_read_sector(SB_LBA, buf) != 0)
    {
        print_color("\nWarning: no disk detected - storage is disabled.\n", COLOR_LIGHT_RED);
        disk_ok = 0;
        return;
    }

    disk_ok = 1;

    superblock_t* sb = (superblock_t*)buf;

    if(sb->magic != FS_MAGIC)
    {

        needs_signup = 1;
    }
    else
    {
        str_copy_n(username, sb->username, MAX_USERNAME);
        str_copy_n(password, sb->password, MAX_PASSWORD);
        fs_read_table();
    }
}



void fs_signup()
{
    char new_user[MAX_USERNAME];
    char new_pass[MAX_PASSWORD];
    char confirm_pass[MAX_PASSWORD];

    clear();

    print_color("Welcome to ZeaxOS - first time setup\n\n", COLOR_LIGHT_CYAN);

    while(1)
    {
        print("Choose a username: ");
        read_line(new_user, MAX_USERNAME, 0);

        if(str_len(new_user) == 0)
        {
            print_color("\nUsername cannot be empty\n\n", COLOR_LIGHT_RED);
            continue;
        }

        break;
    }

    while(1)
    {
        print("\nChoose a password: ");
        read_line(new_pass, MAX_PASSWORD, 1);

        print("\nConfirm password: ");
        read_line(confirm_pass, MAX_PASSWORD, 1);

        if(strcmp(new_pass, confirm_pass) != 0)
        {
            print_color("\nPasswords don't match, try again\n\n", COLOR_LIGHT_RED);
            continue;
        }

        break;
    }

    str_copy_n(username, new_user, MAX_USERNAME);
    str_copy_n(password, new_pass, MAX_PASSWORD);

    fs_format();
    needs_signup = 0;

    print_color("\n\nAccount created! Booting...\n", COLOR_LIGHT_GREEN);
}



void fs_login()
{
    char try_user[MAX_USERNAME];
    char try_pass[MAX_PASSWORD];

    while(1)
    {
        clear();

        print_color("ZeaxOS Login\n\n", COLOR_LIGHT_CYAN);

        print("Username: ");
        read_line(try_user, MAX_USERNAME, 0);

        print("\nPassword: ");
        read_line(try_pass, MAX_PASSWORD, 1);

        if(strcmp(try_user, username) == 0 && strcmp(try_pass, password) == 0)
        {
            print_color("\n\nLogin successful.\n", COLOR_LIGHT_GREEN);
            return;
        }

        print_color("\n\nIncorrect username or password. Press any key to try again...", COLOR_LIGHT_RED);

        while(keyboard() == 0)
            ;
    }
}


int fs_find(char* name)
{
    for(int i = 0; i < MAX_FILES; i++)
    {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0)
            return i;
    }

    return -1;
}


int fs_free_slot()
{
    for(int i = 0; i < MAX_FILES; i++)
    {
        if(!file_table[i].used)
            return i;
    }

    return -1;
}



void file_create(char* name)
{
    if(!disk_ok)
    {
        print_color("\nStorage unavailable (no disk attached)", COLOR_LIGHT_RED);
        return;
    }

    if(str_len(name) == 0)
    {
        print_color("\nUsage: file -C <filename>", COLOR_LIGHT_RED);
        return;
    }

    if(str_len(name) >= MAX_FILENAME)
    {
        print_color("\nFilename too long", COLOR_LIGHT_RED);
        return;
    }

    if(fs_find(name) != -1)
    {
        print_color("\nFile already exists: ", COLOR_LIGHT_RED);
        print(name);
        return;
    }

    int slot = fs_free_slot();

    if(slot == -1)
    {
        print_color("\nNo free space for a new file", COLOR_LIGHT_RED);
        return;
    }

    str_copy_n(file_table[slot].name, name, MAX_FILENAME);
    file_table[slot].size = 0;
    file_table[slot].used = 1;

    static uint8_t empty[SECTOR_SIZE];

    for(int i = 0; i < SECTOR_SIZE; i++)
        empty[i] = 0;

    ata_write_sector(FILE_DATA_LBA_START + slot, empty);

    fs_write_table();

    print_color("\nCreated file: ", COLOR_LIGHT_GREEN);
    print(name);
}



void file_delete(char* name)
{
    if(!disk_ok)
    {
        print_color("\nStorage unavailable (no disk attached)", COLOR_LIGHT_RED);
        return;
    }

    if(str_len(name) == 0)
    {
        print_color("\nUsage: file -R <filename>", COLOR_LIGHT_RED);
        return;
    }

    int slot = fs_find(name);

    if(slot == -1)
    {
        print_color("\nFile not found: ", COLOR_LIGHT_RED);
        print(name);
        return;
    }

    file_table[slot].used = 0;
    file_table[slot].size = 0;
    file_table[slot].name[0] = 0;

    fs_write_table();

    print_color("\nDeleted file: ", COLOR_LIGHT_GREEN);
    print(name);
}



void file_edit(char* name)
{
    if(!disk_ok)
    {
        print_color("\nStorage unavailable (no disk attached)", COLOR_LIGHT_RED);
        return;
    }

    if(str_len(name) == 0)
    {
        print_color("\nUsage: file -E <filename>", COLOR_LIGHT_RED);
        return;
    }

    int slot = fs_find(name);

    if(slot == -1)
    {
        print_color("\nFile not found: ", COLOR_LIGHT_RED);
        print(name);
        print("\nUse 'file -C ");
        print(name);
        print("' to create it first");
        return;
    }

    static uint8_t buf[SECTOR_SIZE];

    ata_read_sector(FILE_DATA_LBA_START + slot, buf);

    print("\nEditing ");
    print(name);
    print(" (ESC to save & exit)\n\n");

    int size = file_table[slot].size;
    int session_start = size;

    for(int i = 0; i < size; i++)
        putchar(buf[i]);

    while(1)
    {
        char c = keyboard();

        if(c == 0)
            continue;

        if(c == 27)
            break;

        if(c == '\b')
        {
            if(size > session_start)
            {
                size--;

                cursor--;
                vga[cursor] = ' ' | ((uint16_t)current_color << 8);
                update_cursor();
            }

            continue;
        }

        if(size < SECTOR_SIZE)
        {
            buf[size] = c;
            size++;

            putchar(c);
        }
    }

    file_table[slot].size = size;

    ata_write_sector(FILE_DATA_LBA_START + slot, buf);
    fs_write_table();

    print_color("\n\nSaved.", COLOR_LIGHT_GREEN);
}



void file_list()
{
    int found = 0;

    print("\nFiles:");

    for(int i = 0; i < MAX_FILES; i++)
    {
        if(file_table[i].used)
        {
            print("\n  ");
            print(file_table[i].name);
            found = 1;
        }
    }

    if(!found)
        print("\n  (none)");
}



void print_prompt()
{
    print("\n\n");
    set_color(COLOR_LIGHT_GREEN);
    print(username);
    print("@zeaxos:~$ ");
    set_color(COLOR_LIGHT_GREY);
}



void shell()
{
    char command[64];

    int pos = 0;


    print_prompt();


    while(1)
    {
        char c = keyboard();


        if(c == 0)
            continue;



        if(c == '\n')
        {
            command[pos] = 0;

            char* argv[8];
            int argc = split_args(command, argv, 8);


            if(argc == 0)
            {

            }

            else if(strcmp(argv[0], "help") == 0)
            {
                print("\nCommands:");
                print("\n help");
                print("\n reboot");
                print("\n off");
                print("\n user -name <name>   change your username");
                print("\n file -C <name>      create a file");
                print("\n file -E <name>      edit a file");
                print("\n file -L             list files");
                print("\n file -R <name>      remove a file");
                print("\n (Page Up / Page Down to scroll the terminal)");
            }


            else if(strcmp(argv[0], "reboot") == 0)
            {
                reboot();
            }


            else if(strcmp(argv[0], "off") == 0)
            {
                shutdown();
            }


            else if(strcmp(argv[0], "user") == 0)
            {
                if(argc < 3 || strcmp(argv[1], "-name") != 0)
                {
                    print_color("\nUsage: user -name <newname>", COLOR_LIGHT_RED);
                }
                else
                {
                    str_copy_n(username, argv[2], MAX_USERNAME);
                    fs_write_superblock();

                    print_color("\nUsername changed to: ", COLOR_LIGHT_GREEN);
                    print(username);
                }
            }


            else if(strcmp(argv[0], "file") == 0)
            {
                if(argc < 2)
                {
                    print_color("\nUsage: file -C|-E|-L|-R <filename>", COLOR_LIGHT_RED);
                }
                else if(strcmp(argv[1], "-C") == 0)
                {
                    if(argc < 3)
                        print_color("\nUsage: file -C <filename>", COLOR_LIGHT_RED);
                    else
                        file_create(argv[2]);
                }
                else if(strcmp(argv[1], "-E") == 0)
                {
                    if(argc < 3)
                        print_color("\nUsage: file -E <filename>", COLOR_LIGHT_RED);
                    else
                        file_edit(argv[2]);
                }
                else if(strcmp(argv[1], "-L") == 0)
                {
                    file_list();
                }
                else if(strcmp(argv[1], "-R") == 0)
                {
                    if(argc < 3)
                        print_color("\nUsage: file -R <filename>", COLOR_LIGHT_RED);
                    else
                        file_delete(argv[2]);
                }
                else
                {
                    print_color("\nUnknown file option: ", COLOR_LIGHT_RED);
                    print(argv[1]);
                }
            }


            else
            {
                print_color("\nUnknown command", COLOR_LIGHT_RED);
            }


            print_prompt();

            pos = 0;
        }



        else if(c == '\b')
        {
            if(pos > 0)
            {
                pos--;
                cursor--;

                vga[cursor] = ' ' | ((uint16_t)current_color << 8);

                update_cursor();
            }
        }



        else
        {
            if(pos < 63)
            {
                command[pos++] = c;

                putchar(c);
            }
        }
    }
}


void zeax_logo()
{
    print("                                                \n");
    print("                            ==============      \n");
    print("          ===========    =================      \n");
    print("      =================  ====+#######*======.   \n");
    print("      ====#########====   ===#########==========\n");
    print("      ===##########==========##########=========\n");
    print("      ====##########==========#########====     \n");
    print(" ==========##########===   ===#########====:    \n");
    print("===========##########====  ========+========    \n");
    print("====    ====+#===========   ==============      \n");
    print("        ===============                         \n");
    print("         ===-                                    \n");
    print("                                                \n");
}

void kernel_main()
{
    clear();

    fs_init();

    if(needs_signup)
    {
        fs_signup();
    }
    else if(disk_ok)
    {
        fs_login();
    }

    clear();

    set_color(COLOR_LIGHT_GREEN);
    zeax_logo();
    set_color(COLOR_LIGHT_GREY);

    print_color("Welcome to ZeaxOS!\n\n", COLOR_LIGHT_CYAN);

    print("Kernel: ");
    print_color("v1.2.1\n", COLOR_YELLOW);

    print("OS: ");
    print_color("Zeax\n", COLOR_YELLOW);

    print("RAM: ");
    set_color(COLOR_YELLOW);
    print_number(get_ram_kb() / 1024);
    print(" MB\n");
    set_color(COLOR_LIGHT_GREY);


    shell();
}
