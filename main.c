#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
// 在全局变量部分添加
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
// =============== 提前定义的全局类型 ===============
// 棋子类型
typedef enum {
    EMPTY = 0,//空位 值为0
    BLACK = 1,//黑棋 值为1
    WHITE = 2//白棋 值为2
} Stone;// 棋子类型
// 在全局变量区域添加新变量
static int game_over = 0;  // 标记游戏是否结束
static int waiting_for_opponent = 0;  // 标记是否在等待对手
static lv_obj_t * status_label = NULL;  // 状态显示标签

// 添加全局变量
static char network_message[1024] = {0};// 存储网络消息
static int network_message_pending = 0;// 标记是否有待处理的网络消息
// 网络相关全局变量
pthread_t network_thread;// 网络线程
bool network_running = false;// 网络线程运行标志
// ========== 全局变量 ==========
int sockfd = -1;  // 初始化 sockfd
lv_obj_t * global_canvas = NULL;// 全局画布对象
Stone goban_board[15][15] = {0};// 棋盘数组
Stone current_player = BLACK;// 当前玩家
// 在全局变量部分添加
pthread_mutex_t network_mutex = PTHREAD_MUTEX_INITIALIZER;// 网络互斥锁
// AI难度枚举
typedef enum {
    AI_EASY = 1,// AI简单难度 值为1
    AI_MEDIUM = 2,// AI中等难度 值为2
    AI_HARD = 3// AI难度级别 值为3
} ai_difficulty_t;// AI难度类型
// 网络对战相关
bool is_network_game = false;// 是否为网络对战
bool is_my_turn = false;// 是否轮到自己下棋
int network_fd = -1;// 网络套接字

// 网络输入框（用于连接服务器）
lv_obj_t *network_ip_ta = NULL;// 网络IP输入框
lv_obj_t *network_port_ta = NULL;// 网络端口输入框
// 添加网络线程函数声明
void *network_thread_func(void *arg);
// =============== Zobrist哈希相关定义 ===============
#define TT_SIZE (1 << 16) // 置换表大小
#define INFINITY 1000000  // 极大值

typedef enum {
    EXACT = 0,// 精确值
    LOWER_BOUND = 1,// 搜索结果上界
    UPPER_BOUND = 2// 搜索结果下界
} TTFlag;// 置换表标志枚举

typedef struct {
    uint64_t key;// 哈希键
    int depth;// 搜索深度
    int score;// 评估分数
    TTFlag flag;// 搜索结果类型
} TTEntry;// 置换表条目结构体

TTEntry transpositionTable[TT_SIZE] = {0};// 置换表
uint64_t zobristTable[15][15][3];// Zobrist哈希表
uint64_t currentHash = 0;// 当前哈希值

// 结构体：用于传递用户名和密码输入框到回调函数
typedef struct {
    lv_obj_t *username_ta;/// 用户名输入框
    lv_obj_t *password_ta;// 密码输入框
} login_data_t;// 登录数据结构体


static lv_style_t style_big_text;// 大字体样式

lv_obj_t * game_btn = NULL;// 游戏按钮
uint8_t selected_rounds_index = 0;// 选择的轮数索引
lv_obj_t * score_label = NULL;// 分数标签
lv_obj_t * rounds_dropdown = NULL;// 轮数下拉框
int player1_score = 0;// 玩家1分数
int player2_score = 0;// 玩家2分数
int current_round = 0;// 当前局数
int total_rounds = 1;// 默认1局

// AI相关全局变量
bool ai_mode_enabled = false; // AI模式开关
int ai_player = WHITE;         // 当前AI玩家
ai_difficulty_t current_ai_difficulty = AI_MEDIUM; // 默认中等难度
lv_obj_t* current_msgbox = NULL; // 当前显示的消息框
int ai_direction_x[4] = {1, 0, 1, 1}; // 四个方向的x增量
int ai_direction_y[4] = {0, 1, 1, -1};// 四个方向的y增量

// ========== test.c 算法相关定义 ==========
#define SIZE 15// 棋盘大小
#define MAX_CHESS (SIZE * SIZE)// 最大棋子数

// 棋盘状态常量
enum {
    EMPTY_TEST = 0, // 空位
    PLAYER_TEST = 1, // 玩家棋子 (○)
    COMPUTER_TEST = -1 // 电脑棋子 (●)
};

// 难度级别
enum {
    EASY_TEST = 1,
    MEDIUM_TEST = 2,
    HARD_TEST = 3
};

// 四个判断方向: 横、纵、主对角、次对角
const int directions_test[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

// test.c算法相关的全局变量
int test_board[SIZE][SIZE];// 棋盘
int emptyCount_test;// 空位计数

// 当前选择的test算法难度
int current_test_difficulty = MEDIUM_TEST;// 默认中等难度

// ========== 完整函数声明 ==========
// 在函数声明部分添加
int start_client(const char *ip, const char *port);// 启动客户端
void login_textarea_cb(lv_event_t * e);// 登录文本框回调
void login_btn_event_cb(lv_event_t * e);// 登录按钮回调
void return_to_login_cb(lv_event_t * e);// 返回登录回调
void create_login_ui();// 创建登录界面
void main_interface();// 主界面
void place_stone_on_canvas(lv_obj_t *canvas, int row, int col, Stone player);// 在画布上放置棋子,参数：画布对象，行，列，棋子类型
void on_canvas_click(lv_event_t * e);       // 画布点击事件回调
Stone check_winner(int row, int col);       // 检查胜利条件
void on_retry_btn_click(lv_event_t * e);    // 重试按钮点击回调
void on_rounds_changed(lv_event_t * e);     // 轮数改变回调
void on_restart_match_cb(lv_event_t * e);   // 重新开始对局回调
static void auto_next_round_cb(lv_timer_t * timer);// 自动进入下一轮回调
void clear_error_hint(lv_event_t * e);      // 清除错误提示
void main_menu_interface(void);             // 主菜单界面
void enter_game_cb(lv_event_t * e);         // 进入游戏回调
void back_to_login_cb(lv_event_t * e);      // 返回登录界面回调

void network_game_interface(lv_event_t * e);// 网络对战界面
void connect_to_server_cb(lv_event_t * e);  // 连接服务器回调

void init_styles();// 初始化样式
int evaluate_position(int x, int y);        // 评估位置分数
void ai_make_move(lv_timer_t * timer);      // AI落子回调

// AI相关函数声明
lv_obj_t* ai_mode_create_button(lv_obj_t* parent);// 创建AI模式按钮
void ai_mode_enable_cb(lv_event_t* e);            // 启用AI模式回调
void ai_difficulty_btn_cb(lv_event_t* e);          // AI难度按钮回调
void apply_difficulty_cb(lv_event_t* e);           // 应用难度回调

// test.c 算法函数声明
void initBoard_test();                                          // 初始化棋盘
bool inBoard_test(int x, int y);                                 // 检查位置是否在边界内             
int checkLine_test(int x, int y, int dx, int dy, int player);   // 检查指定方向的连子数
bool isWin_test(int x, int y, int player);                        // 检查是否胜利
int evalScore_test(int x, int y, int player);                     // 评估当前位置的分数
int advancedEvalScore_test(int x, int y, int player);               // 增强版评估函数
void computerMoveEasy_test(int* computerX, int* computerY);         // 简单电脑模式
void computerMoveMedium_test(int* computerX, int* computerY);       // 中等电脑模式
void computerMoveHard_test(int* computerX, int* computerY);         // 困难电脑模式
void computerMove_test(int* computerX, int* computerY, int difficulty); // 电脑模式        

// 常量
#ifndef LV_CLAMP// 如果未定义LV_CLAMP宏
#define LV_CLAMP(min, val, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))// 限制值在[min, max]范围内参数：min, val, max
#endif

#define BOARD_SIZE_PIXEL  490           // 棋盘像素大小
#define CELL_SIZE         35            // 单元格大小

// ==================== 从test.c移植的算法实现 ====================

// 初始化棋盘
void initBoard_test() 
{
    for (int i = 0; i < SIZE; i++) 
    {
        for (int j = 0; j < SIZE; j++) 
        {
            test_board[i][j] = EMPTY_TEST;// 设置为空位，参数：行，列 EMPTY_TEST = 0
        }
    }
    emptyCount_test = MAX_CHESS; // 初始化空位计数 参数：MAX_CHESS = SIZE * SIZE
}

// 检查位置是否在边界内
bool inBoard_test(int x, int y) //
{
    return x >= 0 && x < SIZE && y >= 0 && y < SIZE;// 参数：行，列
}

// 检查从点(x,y)指定方向是否有连续n个相同类型棋子
int checkLine_test(int x, int y, int dx, int dy, int player) // 参数：起始点(x,y)，方向(dx,dy)，玩家类型
{
    int count = 1; // 包括当前点
    
    // 正方向检查
    for (int i = 1; i < 5; i++) 
    {
        int nx = x + i * dx;
        int ny = y + i * dy;
        if (!inBoard_test(nx, ny) || test_board[nx][ny] != player) // 超出边界或不连续
        {
            break;
        }
        count++;// 增加计数
    }
    
    // 反方向检查
    for (int i = 1; i < 5; i++) 
    {
        int nx = x - i * dx;// 计算x新坐标
        int ny = y - i * dy;// 计算y新坐标
        if (!inBoard_test(nx, ny) || test_board[nx][ny] != player) // 超出边界或不连续
        {
            break;
        }
        count++;
    }
    
    return count;
}

// 基于给定点(x,y)检查连五或胜利状态
bool isWin_test(int x, int y, int player) // 参数：点(x,y)，玩家类型
{
    if (player == EMPTY_TEST) return false;// 空位不可能获胜
    
    for (int i = 0; i < 4; i++) 
    {
        if (checkLine_test(x, y, directions_test[i][0], directions_test[i][1], player) >= 5) // 连续5个或更多
        {
            return true;
        }
    }
    return false;
}

// 简单的评估：检查此处在某个位置是否能形成/阻挡重要连接,分数是综合评估分  
int evalScore_test(int x, int y, int player) 
{
    if (!inBoard_test(x, y) || test_board[x][y] != EMPTY_TEST)// 检查位置有效性
        return INT_MIN;// 无效位置返回最小值
        
    int score = 0;// 位置分数
    
    // 检查各个方向的连子情况
    for (int i = 0; i < 4; i++)
    {
        int lineScore = checkLine_test(x, y, directions_test[i][0], directions_test[i][1], player);// 计算该方向的连子数
        if (lineScore >= 5) 
        {
            score += 100000; // 连五
        } 
        else if (lineScore == 4) 
        {
            score += 10000;  // 活四
        } 
        else if (lineScore == 3) 
        {
            score += 1000;   // 活三
        } 
        else if (lineScore == 2) 
        {
            score += 100;    // 活二
        }
    }
    
    return score;
}

// 困难难度AI（增强版评估函数）
int advancedEvalScore_test(int x, int y, int player) 
{
    if (!inBoard_test(x, y) || test_board[x][y] != EMPTY_TEST)
        return INT_MIN;
        
    int score = 0;
    
    // 检查各个方向的连子情况
    for (int i = 0; i < 4; i++) 
    {
        int lineScore = checkLine_test(x, y, directions_test[i][0], directions_test[i][1], player);// 计算该方向的连子数
        if (lineScore >= 5) 
        {
            score += 1000000; // 连五
        } 
        else if (lineScore == 4) 
        {
            score += 100000;  // 活四
        } else if (lineScore == 3) 
        {
            score += 10000;   // 活三
        } 
        else if (lineScore == 2) 
        {
            score += 1000;    // 活二
        }
    }
    
    // 增加位置权重（中心位置更重要）
    int centerX = SIZE / 2;// 棋盘中心X坐标
    int centerY = SIZE / 2;// 棋盘中心Y坐标
    int distance = abs(x - centerX) + abs(y - centerY);// 曼哈顿距离
    score += (SIZE - distance) * 10;// 中心位置加分，距离越近分数越高
    
    return score;// 返回综合分数
}

// 简单AI（随机下棋）
void computerMoveEasy_test(int* computerX, int* computerY) 
{
    int emptyPositions[SIZE*SIZE][2];// 存储所有空位坐标
    int count = 0;
    
    // 收集所有空位
    for (int i = 0; i < SIZE; i++) 
    {
        for (int j = 0; j < SIZE; j++) 
        {
            if (test_board[i][j] == EMPTY_TEST) // 为空位
            {
                emptyPositions[count][0] = i;// 行坐标
                emptyPositions[count][1] = j;// 列坐标
                count++;
            }
        }
    }
    
    // 随机选择一个空位
    if (count > 0) 
    {
        int randomIndex = rand() % count;
        *computerX = emptyPositions[randomIndex][0];// 选择的行
        *computerY = emptyPositions[randomIndex][1];// 选择的列
        test_board[*computerX][*computerY] = COMPUTER_TEST;// 放置电脑棋子
        emptyCount_test--;
    }
}

// 中等难度AI
void computerMoveMedium_test(int* computerX, int* computerY) 
{
    int maxScore = -1;// 最大分数
    int bestX = SIZE/2, bestY = SIZE/2; // 默认下在中心
    
    // 如果中心位置为空，优先下在中心
    if (test_board[bestX][bestY] == EMPTY_TEST) // 中心位置为空
    {
        *computerX = bestX;// 放置电脑棋子
        *computerY = bestY;// 放置电脑棋子
        test_board[*computerX][*computerY] = COMPUTER_TEST;// 放置电脑棋子
        emptyCount_test--;// 空位计数减一
        return;
    }
    
    // 遍历所有空位寻找最佳位置
    for (int i = 0; i < SIZE; i++) 
    {
        for (int j = 0; j < SIZE; j++) 
        {
            if (test_board[i][j] == EMPTY_TEST) 
            {
                // 1. 检查AI是否能赢
                test_board[i][j] = COMPUTER_TEST;
                if (isWin_test(i, j, COMPUTER_TEST)) 
                {
                    test_board[i][j] = EMPTY_TEST;
                    *computerX = i;
                    *computerY = j;
                    test_board[*computerX][*computerY] = COMPUTER_TEST;
                    emptyCount_test--;
                    return;
                }
                test_board[i][j] = EMPTY_TEST;
                
                // 2. 检查是否需要阻止玩家获胜
                test_board[i][j] = PLAYER_TEST;
                if (isWin_test(i, j, PLAYER_TEST)) 
                {
                    test_board[i][j] = EMPTY_TEST;
                    *computerX = i;
                    *computerY = j;
                    test_board[*computerX][*computerY] = COMPUTER_TEST;
                    emptyCount_test--;
                    return;
                }
                test_board[i][j] = EMPTY_TEST;
                
                // 3. 评估位置分数
                int computerScore = evalScore_test(i, j, COMPUTER_TEST);
                int playerScore = evalScore_test(i, j, PLAYER_TEST);
                int totalScore = computerScore + playerScore;
                
                if (totalScore > maxScore) 
                {
                    maxScore = totalScore;
                    bestX = i;
                    bestY = j;
                }
            }
        }
    }
    
    *computerX = bestX;
    *computerY = bestY;
    test_board[*computerX][*computerY] = COMPUTER_TEST;
    emptyCount_test--;
}

// 困难难度AI
void computerMoveHard_test(int* computerX, int* computerY) 
{
    int maxScore = INT_MIN;
    int bestX = SIZE/2, bestY = SIZE/2; // 默认下在中心
    
    // 如果中心位置为空，优先下在中心
    if (test_board[bestX][bestY] == EMPTY_TEST) 
    {
        *computerX = bestX;
        *computerY = bestY;
        test_board[*computerX][*computerY] = COMPUTER_TEST;
        emptyCount_test--;// 空位计数减一
        return;
    }
    
    // 遍历所有空位寻找最佳位置
    for (int i = 0; i < SIZE; i++) 
    {
        for (int j = 0; j < SIZE; j++) 
        {
            if (test_board[i][j] == EMPTY_TEST) 
            {
                // 1. 检查AI是否能赢
                test_board[i][j] = COMPUTER_TEST;
                if (isWin_test(i, j, COMPUTER_TEST)) 
                {
                    test_board[i][j] = EMPTY_TEST;
                    *computerX = i;
                    *computerY = j;
                    test_board[*computerX][*computerY] = COMPUTER_TEST;
                    emptyCount_test--;
                    return;
                }
                test_board[i][j] = EMPTY_TEST;
                
                // 2. 检查是否需要阻止玩家获胜
                test_board[i][j] = PLAYER_TEST;// 模拟玩家落子
                if (isWin_test(i, j, PLAYER_TEST)) 
                {
                    test_board[i][j] = EMPTY_TEST;
                    *computerX = i;
                    *computerY = j;
                    test_board[*computerX][*computerY] = COMPUTER_TEST;
                    emptyCount_test--;
                    return;
                }
                test_board[i][j] = EMPTY_TEST;
                
                // 3. 使用增强版评估函数
                int computerScore = advancedEvalScore_test(i, j, COMPUTER_TEST);// 电脑分数
                int playerScore = advancedEvalScore_test(i, j, PLAYER_TEST);// 玩家分数
                int totalScore = computerScore + playerScore;// 综合分数
                
                if (totalScore > maxScore) // 找到最大的分数
                {
                    maxScore = totalScore;// 最大分数
                    bestX = i;// 最佳X坐标
                    bestY = j;// 最佳Y坐标
                }
            }
        }
    }
    
    *computerX = bestX;// 输出最佳坐标
    *computerY = bestY;// 输出最佳坐标
    test_board[*computerX][*computerY] = COMPUTER_TEST;// 放置电脑棋子
    emptyCount_test--;// 空位计数减一
}

// 根据难度选择AI
void computerMove_test(int* computerX, int* computerY, int difficulty) 
{
    switch(difficulty) 
    {
        case EASY_TEST:
            computerMoveEasy_test(computerX, computerY);
            break;
        case MEDIUM_TEST:
            computerMoveMedium_test(computerX, computerY);
            break;
        case HARD_TEST:
            computerMoveHard_test(computerX, computerY);
            break;
        default:
            computerMoveMedium_test(computerX, computerY);
            break;
    }
}
int main(void)
{
    lv_init();
    init_styles();
    
    // 初始化Zobrist哈希
    srand(42); // 固定种子确保可重复性
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            for (int k = 0; k < 3; k++) {
                uint64_t rand1 = (uint64_t)rand() << 32;
                uint64_t rand2 = rand();
                zobristTable[i][j][k] = rand1 | rand2;
            }
        }
    }
    
    // 初始哈希值（空棋盘）
    currentHash = 0;
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            currentHash ^= zobristTable[i][j][EMPTY];
        }
    }
    
    // 初始化test.c算法的棋盘
    initBoard_test();

    /* Linux frame buffer device init */
    lv_display_t * disp = lv_linux_fbdev_create();
    if (disp) 
    {
        lv_linux_fbdev_set_file(disp, "/dev/fb0");
    }
    
    lv_indev_t * indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");
    if (!indev) 
    {
        printf("Warning: Failed to create input device\n");
    }

    create_login_ui();
   
    while(1) 
    {
        // 安全地处理网络消息（在主线程中）
        if (network_message_pending) 
        {
            // 添加额外的安全检查
            if (network_message[sizeof(network_message) - 1] != '\0') 
            {
                network_message[sizeof(network_message) - 1] = '\0';
            }
            process_network_message();
        }
        
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
// =============== Zobrist和MTD(f)实现 ===============
void update_hash(int row, int col, Stone old_state, Stone new_state) 
{
    currentHash ^= zobristTable[row][col][old_state];
    currentHash ^= zobristTable[row][col][new_state];
}

TTEntry *tt_get(uint64_t key) 
{
    TTEntry *entry = &transpositionTable[key % TT_SIZE];
    return (entry->key == key) ? entry : NULL;
}

void tt_store(uint64_t key, int depth, int score, TTFlag flag) 
{
    TTEntry *entry = &transpositionTable[key % TT_SIZE];
    entry->key = key;
    entry->depth = depth;
    entry->score = score;
    entry->flag = flag;
}

// 其他函数保持不变
void init_styles() 
{
    lv_style_init(&style_big_text);
    lv_style_set_text_font(&style_big_text, &lv_font_montserrat_24);
    lv_style_set_text_letter_space(&style_big_text, 2);
    lv_style_set_text_line_space(&style_big_text, 4);
}

// —————————————————————— 软键盘事件回调 —————————————————————— //
void login_textarea_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * keyboard = lv_event_get_user_data(e);
    lv_obj_t * textarea = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) 
    {
        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, textarea);
    }
    else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY) 
    {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(keyboard, NULL);
    }
}
// —————————————————————— 登录按钮点击事件 —————————————————————— //
/**
 * @brief 登录按钮的事件回调函数。
 * 
 * 当用户点击登录按钮时触发该回调函数，获取用户名和密码输入框中的内容，
 * 并进行简单的验证（硬编码为 "admin"/"123456"）。如果验证成功，则跳转到主菜单界面；
 * 如果失败，则清空输入框内容并提示用户重新输入。
 * 
 * @param e 指向事件对象的指针，包含事件相关信息。
 * 
 * @note 该函数假设登录界面中的用户名和密码输入框已通过 user_data 绑定到 login_data_t 结构体。
 * @note 登录成功后会调用 main_menu_interface() 显示主菜单。
 * @note 登录失败时会强制刷新输入框的 placeholder 文本以提示用户。
 */
void login_btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        login_data_t * data = (login_data_t *)lv_event_get_user_data(e);
        const char * username = lv_textarea_get_text(data->username_ta);
        const char * password = lv_textarea_get_text(data->password_ta);

        printf("=== 用户尝试登录 ===\n");
        printf("用户名: %s\n", username);
        printf("密码: %s\n", password);

        // 简单验证用户名和密码是否匹配硬编码值
        bool is_valid_username = strcmp(username, "1") == 0;
        bool is_valid_password = strcmp(password, "1") == 0;

        if (is_valid_username && is_valid_password) 
        {
            printf("✅ 登录成功！\n");
            lv_obj_clean(lv_screen_active());
             main_menu_interface();
        } 
        else 
        {
            printf("❌ 登录失败！\n");

            // 清空输入框内容以便重新输入
            lv_textarea_set_text(data->username_ta, "");
            lv_textarea_set_text(data->password_ta, "");

            // 强制刷新 placeholder：先设空格，再设提示（避免 LVGL 缓存）
            if (!is_valid_username) 
            {
                lv_textarea_set_placeholder_text(data->username_ta, " ");
                lv_textarea_set_placeholder_text(data->username_ta, "- Please re-enter");
            } 
            else 
            {
                // 如果用户名正确，恢复原始提示
                lv_textarea_set_placeholder_text(data->username_ta, "Enter your username");
            }

            if (!is_valid_password) 
            {
                lv_textarea_set_placeholder_text(data->password_ta, " ");
                lv_textarea_set_placeholder_text(data->password_ta, "- Please re-enter");
            } 
            else 
            {
                // 如果密码正确，恢复原始提示
                lv_textarea_set_placeholder_text(data->password_ta, "Enter your password");
            }

            // 强制重绘，确保 UI 更新
            lv_obj_invalidate(data->username_ta);
            lv_obj_invalidate(data->password_ta);
        }

        free(data);
    }
}

// —————————————————————— 返回登录界面按钮事件 —————————————————————— //
// 修改 return_to_login_cb 函数，确保正确清理网络连接
void return_to_login_cb(lv_event_t * e)
{
    printf("Return to login clicked\n");

    if (is_network_game) 
    {
        printf("Closing network connection\n");
        network_running = false;

        if (sockfd != -1) 
        {
            close(sockfd);
            sockfd = -1;
        }

        usleep(200 * 1000);
        is_network_game = false;
        is_my_turn = false;
        game_over = 0;
        waiting_for_opponent = 0;
        printf("Network connection closed\n");
    }

    lv_obj_clean(lv_screen_active());
    global_canvas = NULL;
    network_ip_ta = NULL;
    network_port_ta = NULL;
    status_label = NULL;
    main_menu_interface();
}

// 回调：返回登录界面
void back_to_login_cb(lv_event_t * e)
{
    static bool in_progress = false;
    if (in_progress) return;
    in_progress = true;
    printf("Back to login clicked\n");
    // 如果是网络对战模式，关闭网络连接
    if (is_network_game) 
    {
        printf("Closing network connection\n");
        network_running = false;
        
        // 等待网络线程结束
        usleep(200000);
        
        if (sockfd != -1) 
        {
            close(sockfd);
            sockfd = -1;
        }
        is_network_game = false;
        is_my_turn = false;
        printf("Network connection closed\n");
    }
    
    lv_obj_clean(lv_screen_active());
    // 重置全局UI指针，防止野指针
    global_canvas = NULL;
    network_ip_ta = NULL;
    network_port_ta = NULL;
    // 其它相关全局指针也可一并重置

    create_login_ui();  // 返回登录界面
    in_progress = false;
}
// —————————————————————— 创建登录界面 —————————————————————— //
void create_login_ui()
{
    lv_obj_t * screen = lv_screen_active();

    // === 设置背景图 1024x600 ===
    lv_obj_t * bg_img = lv_image_create(screen);
    lv_image_set_src(bg_img, "A:./1.bmp");  // 使用你习惯的路径格式
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    // === 创建登录面板 ===
    lv_obj_t * panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 400, 350);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

    // ✅ 修改：面板背景为灰色（原为白色）
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xAB82FF), 0);

    lv_obj_set_style_border_color(panel, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_y(panel, 5, 0);
    lv_obj_set_style_shadow_spread(panel, 3, 0);

    // === 🔧 关键：设置内边距，防止内容贴边或溢出 ===
    lv_obj_set_style_pad_left(panel, 30, 0);
    lv_obj_set_style_pad_right(panel, 30, 0);
    lv_obj_set_style_pad_top(panel, 40, 0);
    lv_obj_set_style_pad_bottom(panel, 40, 0);

    // === 🔥 禁止面板滚动（解决滑动问题）===
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    // === 标题：居中 ===
    lv_obj_t * title = lv_label_create(panel);
    lv_label_set_text(title, "User Login");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    // ✅ 修改：标题文字为白色
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // === 用户名标签 ===
    lv_obj_t * label_user = lv_label_create(panel);
    lv_label_set_text(label_user, "Username:");
    // ✅ 新增：标签文字为白色
    lv_obj_set_style_text_color(label_user, lv_color_white(), 0);
    lv_obj_align_to(label_user, panel, LV_ALIGN_TOP_LEFT, 0, 60);  // 相对 panel 左上

    // === 用户名输入框：使用百分比宽度，避免溢出 ===
    lv_obj_t * ta_username = lv_textarea_create(panel);
    lv_obj_set_width(ta_username, lv_pct(90));  // 占 panel 内宽度的 90%
    lv_obj_align_to(ta_username, label_user, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_textarea_set_one_line(ta_username, true);
    lv_textarea_set_placeholder_text(ta_username, "Enter your username");
    // ✅ 新增：输入框背景为白色
    lv_obj_set_style_bg_color(ta_username, lv_color_white(), 0);

    // === 密码标签 ===
    lv_obj_t * label_pass = lv_label_create(panel);
    lv_label_set_text(label_pass, "Password:");
    // ✅ 新增：标签文字为白色
    lv_obj_set_style_text_color(label_pass, lv_color_white(), 0);
    lv_obj_align_to(label_pass, ta_username, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);

    // === 密码输入框 ===
    lv_obj_t * ta_password = lv_textarea_create(panel);
    lv_obj_set_width(ta_password, lv_pct(90));
    lv_obj_align_to(ta_password, label_pass, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_textarea_set_one_line(ta_password, true);
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_placeholder_text(ta_password, "Enter your password");
    // ✅ 新增：输入框背景为白色
    lv_obj_set_style_bg_color(ta_password, lv_color_white(), 0);

    // === 软键盘 ===
    lv_obj_t * keyboard = lv_keyboard_create(screen);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);

    // 绑定软键盘事件
    lv_obj_add_event_cb(ta_username, login_textarea_cb, LV_EVENT_ALL, keyboard);
    lv_obj_add_event_cb(ta_password, login_textarea_cb, LV_EVENT_ALL, keyboard);

    // === 登录按钮 ===
    login_data_t * data = malloc(sizeof(login_data_t));
    data->username_ta = ta_username;
    data->password_ta = ta_password;

    lv_obj_t * btn = lv_btn_create(panel);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align_to(btn, ta_password, LV_ALIGN_OUT_BOTTOM_MID, 0, 30);
    lv_obj_add_event_cb(btn, login_btn_event_cb, LV_EVENT_CLICKED, data);

    lv_obj_t * btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "LOGIN");
    lv_obj_center(btn_label);

    // 绑定输入事件
    lv_obj_add_event_cb(ta_username, clear_error_hint, LV_EVENT_INSERT, NULL);
    lv_obj_add_event_cb(ta_username, clear_error_hint, LV_EVENT_CLICKED, NULL); // 点击也触发

    lv_obj_add_event_cb(ta_password, clear_error_hint, LV_EVENT_INSERT, NULL);
    lv_obj_add_event_cb(ta_password, clear_error_hint, LV_EVENT_CLICKED, NULL);

    // 设置用户数据用于区分
    lv_obj_set_user_data(ta_username, (void*)0x1);
    lv_obj_set_user_data(ta_password, (void*)0x2);
}

// 回调函数：用户输入时清除错误提示
void clear_error_hint(lv_event_t * e)
{
    lv_obj_t * ta = lv_event_get_target(e);
    const char * text = lv_textarea_get_text(ta);
    const char * placeholder = lv_textarea_get_placeholder_text(ta);

    // 如果 placeholder 是错误提示，则恢复原始提示
    if (strcmp(placeholder, "- Please re-enter") == 0) {
        if (lv_obj_get_user_data(ta) == (void*)0x1) {
            lv_textarea_set_placeholder_text(ta, "Enter your username");
        } else if (lv_obj_get_user_data(ta) == (void*)0x2) {
            lv_textarea_set_placeholder_text(ta, "Enter your password");
        }
        lv_obj_invalidate(ta);
    }
}

/**
 * @brief 主界面初始化函数，用于构建游戏主界面UI元素。
 * 
 * 该函数负责创建并配置主界面的各个组件，包括背景图、AI模式按钮、轮数选择下拉框、
 * 比分标签、棋盘容器及Canvas画布，并在画布上绘制15x15标准围棋棋盘线和天元标记。
 * 同时注册了点击事件以支持落子操作，并提供退出按钮返回登录界面。
 * 
 * @note 本函数不接受参数，也无返回值。
 */
// 修改 main_interface 函数，添加状态标签
void main_interface()
{   
    lv_obj_t * screen = lv_screen_active();
    lv_obj_clean(screen);
    
    // === 设置背景图 1024x600 ===
    lv_obj_t * bg_img = lv_image_create(screen);
    lv_image_set_src(bg_img, "A:./2.bmp");
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    
    // ======== 添加AI模式按钮 ========
    lv_obj_t* ai_btn = ai_mode_create_button(screen);
    
    // === 轮数选择下拉框（左上角）===
    rounds_dropdown = lv_dropdown_create(screen);
    lv_dropdown_set_options(rounds_dropdown, "1\n3\n5");
    lv_obj_align(rounds_dropdown, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_width(rounds_dropdown, 100);
    lv_dropdown_set_selected(rounds_dropdown, selected_rounds_index);
    lv_obj_add_event_cb(rounds_dropdown, on_rounds_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // === 比分标签（右上角）===
    score_label = lv_label_create(screen);
    lv_label_set_text_fmt(score_label, "Player1 Score: %d | Player2 Score: %d", player1_score, player2_score);
    lv_obj_align(score_label, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_16, 0);

    // === 状态标签（顶部中央）===
     status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "Connecting to server...");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF0000), 0); // 红色文本

    // === 棋盘容器 ===
    const int BOARD_SIZE = 490;
    lv_obj_t * board = lv_obj_create(screen);
    lv_obj_set_size(board, BOARD_SIZE, BOARD_SIZE);
    lv_obj_align(board, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(board, lv_color_hex(0xFFEEAD0E), 0);  
    lv_obj_clear_flag(board, LV_OBJ_FLAG_SCROLLABLE);

    // === 创建 Canvas ===
    lv_obj_t * canvas = lv_canvas_create(board);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    size_t buf_size = LV_CANVAS_BUF_SIZE(BOARD_SIZE, BOARD_SIZE, LV_COLOR_FORMAT_ARGB8888, LV_DRAW_BUF_ALIGN);
    void * buf = malloc(buf_size);
    if (buf == NULL) 
    {
        LV_LOG_ERROR("Failed to allocate canvas buffer");
        return;
    }
    
    lv_canvas_set_buffer(canvas, buf, BOARD_SIZE, BOARD_SIZE, LV_COLOR_FORMAT_ARGB8888);
    lv_canvas_fill_bg(canvas, lv_color_hex(0xFFEEAD0E), LV_OPA_COVER);
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    // --- 绘制棋盘线 ---
    static lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_black();
    line_dsc.width = 1;
    line_dsc.opa = LV_OPA_100;

    const int cell_size = BOARD_SIZE / 14;
    const int offset = 1;
    for (int i = 0; i < 15; i++) 
    {
        int pos = i * cell_size;
        line_dsc.p1.x = offset; line_dsc.p1.y = pos + offset;
        line_dsc.p2.x = BOARD_SIZE - offset - 1; line_dsc.p2.y = pos + offset;
        lv_draw_line(&layer, &line_dsc);

        line_dsc.p1.x = pos + offset; line_dsc.p1.y = offset;
        line_dsc.p2.x = pos + offset; line_dsc.p2.y = BOARD_SIZE - offset - 1;
        lv_draw_line(&layer, &line_dsc);
    }

    // --- 绘制天元 ---
    static lv_draw_rect_dsc_t circle_dsc;
    lv_draw_rect_dsc_init(&circle_dsc);
    circle_dsc.bg_color = lv_color_black();
    circle_dsc.radius = LV_RADIUS_CIRCLE;

    int center_x = 7 * cell_size;
    int center_y = 7 * cell_size;
    int r = 3;

    lv_area_t a = {.x1 = center_x - r, .y1 = center_y - r, .x2 = center_x + r, .y2 = center_y + r};
    lv_draw_rect(&layer, &circle_dsc, &a);

    lv_canvas_finish_layer(canvas, &layer);
    lv_obj_invalidate(screen);

    // ===== 落子功能 ====
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas, on_canvas_click, LV_EVENT_CLICKED, NULL);

    // 保存到全局变量
    global_canvas = canvas;

    // === 退出按钮 ===
    lv_obj_t * btn = lv_btn_create(screen); 
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(btn, return_to_login_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "EXIT");
    lv_obj_center(label);
}

void on_canvas_click(lv_event_t * e) 
{
    lv_obj_t * canvas = lv_event_get_target(e);
    if (!canvas) 
    {
        printf("Canvas is NULL\n");
        return;
    }
    
    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) 
    {
        printf("Input device is NULL\n");
        return;
    }
    
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t canvas_area;
    lv_obj_get_coords(canvas, &canvas_area);

    int local_x = point.x - canvas_area.x1;
    int local_y = point.y - canvas_area.y1;

    if (local_x < 0 || local_x > 490 || local_y < 0 || local_y > 490) 
    {
        return;
    }

    int col = (local_x + CELL_SIZE / 2) / CELL_SIZE;
    int row = (local_y + CELL_SIZE / 2) / CELL_SIZE;
    col = LV_CLAMP(0, col, 14);
    row = LV_CLAMP(0, row, 14);

    if (goban_board[row][col] != EMPTY) 
    {
        return;
    }

    // AI模式下检查是否是玩家回合
    if (ai_mode_enabled && current_player == ai_player) 
    {
        LV_LOG_USER("AI mode: not player's turn");
        printf("AI mode: not player's turn\n");
        return;
    }
    
    // 网络对战模式下检查是否是自己的回合
    if (is_network_game)
    {
        if (!is_my_turn) 
        {
            LV_LOG_USER("Network mode: not your turn");
            printf("Network mode: not your turn\n");
            return;
        }
            
        // 发送落子信息到服务器
        char msg[32];
        snprintf(msg, sizeof(msg), "MOVE %02d %02d", row, col);
        printf("Sending move: %s\n", msg);
        if (sockfd != -1) 
        {
            int result = write(sockfd, msg, strlen(msg) + 1);
            if (result > 0) 
            {
                printf("Sent move to server: %s\n", msg);
                // 更新状态为等待对手
                is_my_turn = false;
                waiting_for_opponent = 1;
                if (status_label) 
                {
                    lv_label_set_text(status_label, "Waiting for opponent...");
                }
            } 
            else 
            {
                perror("Failed to send move");
                if (status_label) 
                {
                    lv_label_set_text(status_label, "Connection error!");
                }
            }
        } 
        else 
        {
            printf("Socket is not valid\n");
            if (status_label) 
            {
                lv_label_set_text(status_label, "Connection lost!");
            }
        }
    }

    place_stone_on_canvas(canvas, row, col, current_player);
}
// 修改 place_stone_on_canvas 函数，加强错误检查
void place_stone_on_canvas(lv_obj_t *canvas, int row, int col, Stone player) 
{
    // 加强空指针检查
    if (!canvas) 
    {
        LV_LOG_ERROR("canvas is NULL! row=%d col=%d player=%d", row, col, player);
        printf("canvas is NULL! row=%d col=%d player=%d\n", row, col, player);
        return;
    }
    
    if (row < 0 || row >= 15 || col < 0 || col >= 15) 
    {
        LV_LOG_USER("Invalid position: [%d][%d]", row, col);
        return;
    }
    
    Stone old_state = goban_board[row][col];
    if (old_state != EMPTY) 
    {
        LV_LOG_USER("Position already occupied: %d", old_state);
        return;
    }
    
    LV_LOG_USER("Placing stone at [%d][%d], current player: %s", row, col, 
                player == BLACK ? "Black" : "White");
    
    int x = col * CELL_SIZE;
    int y = row * CELL_SIZE;
    int r = 14;

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    static lv_draw_rect_dsc_t stone_dsc;
    lv_draw_rect_dsc_init(&stone_dsc);
    
    if (player == BLACK) 
    {
        stone_dsc.bg_color = lv_color_black();
        stone_dsc.border_color = lv_color_make(60, 60, 60);
    } 
    else 
    {
        stone_dsc.bg_color = lv_color_white();
        stone_dsc.border_color = lv_color_make(160, 160, 160);
    }
    
    stone_dsc.border_width = 1;
    stone_dsc.radius = LV_RADIUS_CIRCLE;
    stone_dsc.shadow_color = lv_color_make(30, 30, 30);
    stone_dsc.shadow_width = 3;

    lv_area_t a;
    a.x1 = x - r;
    a.y1 = y - r;
    a.x2 = x + r;
    a.y2 = y + r;

    lv_draw_rect(&layer, &stone_dsc, &a);
    lv_canvas_finish_layer(canvas, &layer);
    
    goban_board[row][col] = player;
    update_hash(row, col, old_state, player);
    lv_obj_invalidate(canvas);
    
    Stone winner = check_winner(row, col);
    if (winner != EMPTY) 
    {
        LV_LOG_USER("🎉 Winner: %s", winner == BLACK ? "Black" : "White");
        
        // 移除点击事件
        if (global_canvas) 
        {
            lv_obj_remove_event_cb(global_canvas, on_canvas_click);
        }

        // 更新分数
        if (winner == BLACK) player1_score++;
        else player2_score++;
        
        // 更新分数标签
        if (score_label) 
        {
            lv_label_set_text_fmt(score_label, "Player1 Score: %d | Player2 Score: %d", 
                                  player1_score, player2_score);
        }
        
        current_round++;
        LV_LOG_USER("Current round: %d/%d", current_round, total_rounds);

        // 创建胜利标签（加强检查）
        lv_obj_t * win_label = NULL;
        lv_obj_t * screen = lv_screen_active();
        if (screen) 
        {
            win_label = lv_label_create(screen);
            if (win_label) 
            {
                lv_label_set_text(win_label, winner == BLACK ? "Black Wins!" : "White Wins!");
                lv_obj_align(win_label, LV_ALIGN_TOP_MID, 0, 50);
                lv_obj_set_style_text_font(win_label, &lv_font_montserrat_24, 0);
                lv_obj_set_style_text_color(win_label, lv_color_make(255, 0, 0), 0);
            }
        }

        // 标记游戏结束
        game_over = 1;
        
        if (current_round >= total_rounds) 
        {
            // 创建最终结果标签
            lv_obj_t * final = NULL;
            if (screen) 
            {
                final = lv_label_create(screen);
                if (final) 
                {
                    if (player1_score > player2_score) 
                    {
                        lv_label_set_text(final, "Player1 Wins Match!");
                    } 
                    else if (player1_score < player2_score) 
                    {
                        lv_label_set_text(final, "Player2 Wins Match!");
                    } 
                    else 
                    {
                        lv_label_set_text(final, "Draw Match!");
                    }
                    lv_obj_align(final, LV_ALIGN_CENTER, 0, -50);
                    lv_obj_set_style_text_font(final, &lv_font_montserrat_20, 0);
                    lv_obj_set_style_text_color(final, lv_color_make(178, 58, 238), 0);
                }
            }
            
            // 创建新游戏按钮
            lv_obj_t * btn = NULL;
            if (screen) 
            {
                btn = lv_btn_create(screen);
                if (btn) 
                {
                    lv_obj_set_size(btn, 150, 50);
                    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 50);
                    lv_obj_add_event_cb(btn, on_restart_match_cb, LV_EVENT_CLICKED, NULL);
                    
                    lv_obj_t * label = lv_label_create(btn);
                    if (label) 
                    {
                        lv_label_set_text(label, "New Match");
                        lv_obj_center(label);
                    }
                }
            }
            
            // 更新状态标签
            if (status_label) 
            {
                lv_label_set_text(status_label, winner == BLACK ? "Black wins!" : "White wins!");
            }
        } 
        else 
        {
            // 显示继续下一局按钮
            lv_obj_t * next_btn = NULL;
            lv_obj_t * screen = lv_screen_active();
            if (screen) 
            {
                next_btn = lv_btn_create(screen);
                if (next_btn) 
                {
                    lv_obj_set_size(next_btn, 150, 50);
                    lv_obj_align(next_btn, LV_ALIGN_CENTER, 0, 50);
                    lv_obj_add_event_cb(next_btn, auto_next_round_cb, LV_EVENT_CLICKED, NULL);
                    
                    lv_obj_t * label = lv_label_create(next_btn);
                    if (label) 
                    {
                        lv_label_set_text(label, "Next Round");
                        lv_obj_center(label);
                    }
                }
            }
            
            // 更新状态标签
            if (status_label) 
            {
                lv_label_set_text(status_label, winner == BLACK ? "Black wins!" : "White wins!");
            }
        }
    } 
    else 
    {
        // 没有胜者，切换玩家
        if (!is_network_game) 
        {
            current_player = (current_player == BLACK) ? WHITE : BLACK;
            LV_LOG_USER("🔄 Switching to player: %s", current_player == BLACK ? "Black" : "White");
        }
        
        if (ai_mode_enabled && current_player == ai_player) 
        {
            lv_timer_t* ai_timer = NULL;
            
            switch (current_ai_difficulty) 
            {
                case AI_EASY:
                    ai_timer = lv_timer_create(ai_make_move, 1000, NULL);
                    break;
                case AI_MEDIUM:
                    ai_timer = lv_timer_create(ai_make_move, 1500, NULL);
                    break;
                case AI_HARD:
                    ai_timer = lv_timer_create(ai_make_move, 2000, NULL);
                    break;
                default:
                    ai_timer = lv_timer_create(ai_make_move, 1500, NULL);
                    break;
            }
            
            if (ai_timer) 
            {
                lv_timer_set_repeat_count(ai_timer, 1);
                LV_LOG_USER("✅ AI thinking started (%s)", 
                          current_ai_difficulty == AI_EASY ? "Easy" : 
                          current_ai_difficulty == AI_MEDIUM ? "Medium" : "Hard");
            }
        } 
        else if (is_network_game) 
        {
            // 网络游戏状态下，更新状态标签
            if (status_label) 
            {
                if (is_my_turn) 
                {
                    lv_label_set_text(status_label, "Your turn");
                } 
                else 
                {
                    lv_label_set_text(status_label, "Waiting for opponent...");
                }
            }
        }
    }
}

// 修正后的 place_stone_on_canvas_remote
void place_stone_on_canvas_remote(void *canvas, int row, int col, int player)
{
    place_stone_on_canvas((lv_obj_t *)canvas, row, col, (Stone)player);
    // 切换回合
    if (is_network_game) 
    {
        is_my_turn = true;
    }
    // 只在本地/AI对战时切换 current_player
    if (!is_network_game) 
    {
        current_player = (current_player == BLACK) ? WHITE : BLACK;
        LV_LOG_USER("🔄 切换到玩家: %s", current_player == BLACK ? "黑棋" : "白棋");
    }
}

Stone check_winner(int row, int col)
{
    if (goban_board[row][col] == EMPTY) return EMPTY;
    Stone player = goban_board[row][col];
    int count;

    // 四个方向：横、竖、正斜、反斜
    int dirs[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};

    for (int i = 0; i < 4; i++) 
    {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        count = 1;  // 当前这颗子

        // 正方向延伸（+dx, +dy）
        for (int j = 1; j < 5; j++) 
        {
            int r = row + j * dy;
            int c = col + j * dx;
            if (r < 0 || r >= 15 || c < 0 || c >= 15) break;
            if (goban_board[r][c] == player) count++;
            else break;  // 断了就停
        }

        // 反方向延伸（-dx, -dy）
        for (int j = 1; j < 5; j++) 
        {
            int r = row - j * dy;
            int c = col - j * dx;
            if (r < 0 || r >= 15 || c < 0 || c >= 15) break;
            if (goban_board[r][c] == player) count++;
            else break;  // 断了就停
        }

        if (count >= 5) 
        {
            return player;
        }
    }

    return EMPTY;
}

// 回调：继续挑战
void on_retry_btn_click(lv_event_t * e)
{
    LV_LOG_USER("🔄 用户点击：继续挑战");

    // 清除当前界面
    lv_obj_clean(lv_screen_active());

    // 重新初始化游戏数据
    memset(goban_board, 0, sizeof(goban_board));
    current_player = BLACK;

    // 重新创建游戏界面
    main_interface();
}

void on_rounds_changed(lv_event_t * e) {
    /* 修改：重置哈希值 */
    lv_obj_t * dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);

    selected_rounds_index = selected;
    switch (selected) {
        case 0: total_rounds = 1; break;
        case 1: total_rounds = 3; break;
        case 2: total_rounds = 5; break;
        default: total_rounds = 1; break;
    }

    player1_score = 0;
    player2_score = 0;
    current_round = 0;
    current_player = BLACK;
    memset(goban_board, 0, sizeof(goban_board));
    
    // 重置AI模式
    ai_mode_enabled = false;
    ai_player = WHITE;
    current_ai_difficulty = AI_MEDIUM;
    
    // 重置哈希
    currentHash = 0;
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            currentHash ^= zobristTable[i][j][EMPTY];
        }
    }

    lv_obj_clean(lv_screen_active());
    main_interface();
}

void on_restart_match_cb(lv_event_t * e) {
    /* 修改：重置哈希值 */
    player1_score = 0;
    player2_score = 0;
    current_round = 0;

    memset(goban_board, 0, sizeof(goban_board));
    
    ai_mode_enabled = false;
    ai_player = WHITE;
    current_ai_difficulty = AI_MEDIUM;
    
    // 重置哈希
    currentHash = 0;
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            currentHash ^= zobristTable[i][j][EMPTY];
        }
    }

    lv_obj_clean(lv_screen_active());
    main_interface();
}

static void auto_next_round_cb(lv_timer_t * timer) {
    /* 修改：重置哈希值 */
    current_player = (current_player == BLACK) ? WHITE : BLACK;

    memset(goban_board, 0, sizeof(goban_board));
    
    // 重置哈希
    currentHash = 0;
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < 15; j++) {
            currentHash ^= zobristTable[i][j][EMPTY];
        }
    }

    lv_obj_clean(lv_screen_active());
    main_interface();

    lv_timer_del(timer);
}

void main_menu_interface(void)
{
    lv_obj_t * screen = lv_screen_active();
    
    // 清除旧内容并强制刷新
    lv_obj_clean(screen);
    lv_task_handler();  // 确保清除完成

    // 设置纯色背景防止残影
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // === 背景图 4.bmp ===
    lv_obj_t * bg_img = lv_image_create(screen);
    lv_image_set_src(bg_img, "A:./4.bmp");
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    // === 开始游戏按钮（左上角）===
    lv_obj_t * game_btn = lv_button_create(screen);
    lv_obj_set_size(game_btn, 80, 80);  // 
    lv_obj_align(game_btn, LV_ALIGN_TOP_LEFT, 20, 20);  // 左上角，距离左边和顶部各20px
    lv_obj_clear_flag(game_btn, LV_OBJ_FLAG_SCROLLABLE);

    // 设置按钮为透明，只显示图片
    lv_obj_set_style_bg_opa(game_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(game_btn, 0, 0);
    lv_obj_set_style_shadow_width(game_btn, 0, 0);

    // 添加图片
    lv_obj_t * btn_img = lv_image_create(game_btn);
    lv_image_set_src(btn_img, "A:./3.bmp");  // 图片路径，请确认图片尺寸适合或能良好适配按钮大小
    lv_obj_center(btn_img);

    // 添加点击事件：进入游戏
    lv_obj_add_event_cb(game_btn, enter_game_cb, LV_EVENT_CLICKED, NULL);

        // === 网络对战按钮（左下角）===
    lv_obj_t * network_btn = lv_button_create(screen);
    lv_obj_set_size(network_btn, 100, 40);
    lv_obj_align(network_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_event_cb(network_btn, network_game_interface, LV_EVENT_CLICKED, NULL);
    lv_obj_t * network_label = lv_label_create(network_btn);
    lv_label_set_text(network_label, "Network");
    lv_obj_center(network_label);

    // === 返回登录按钮（右下角）===
    lv_obj_t * back_btn = lv_button_create(screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

    lv_obj_t * label = lv_label_create(back_btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);

    // 添加点击事件：返回登录界面
    lv_obj_add_event_cb(back_btn, back_to_login_cb, LV_EVENT_CLICKED, NULL);
}

// 回调：进入游戏界面
void enter_game_cb(lv_event_t * e)
{
    lv_obj_clean(lv_screen_active());
    main_interface();  // 进入五子棋游戏
}


// 网络对战界面
void network_game_interface(lv_event_t * e)
{
    lv_obj_clean(lv_screen_active());

    lv_obj_t * screen = lv_screen_active();

    // === 背景图 ===
    lv_obj_t * bg_img = lv_image_create(screen);
    lv_image_set_src(bg_img, "A:./2.bmp");
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    // === 软键盘 ===
    lv_obj_t * keyboard = lv_keyboard_create(screen);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER); // 使用数字键盘模式

    // === IP 输入框 ===
    lv_obj_t * ip_label = lv_label_create(screen);
    lv_label_set_text(ip_label, "Server IP:");
    lv_obj_align(ip_label, LV_ALIGN_TOP_MID, 0, 50);

    network_ip_ta = lv_textarea_create(screen);
    lv_textarea_set_one_line(network_ip_ta, true);
    lv_textarea_set_placeholder_text(network_ip_ta, "Enter server IP");
    lv_obj_set_width(network_ip_ta, 200);
    lv_obj_align_to(network_ip_ta, ip_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // 绑定软键盘事件
    lv_obj_add_event_cb(network_ip_ta, login_textarea_cb, LV_EVENT_ALL, keyboard);

    // === Port 输入框 ===
    lv_obj_t * port_label = lv_label_create(screen);
    lv_label_set_text(port_label, "Port:");
    lv_obj_align_to(port_label, network_ip_ta, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

    network_port_ta = lv_textarea_create(screen);
    lv_textarea_set_one_line(network_port_ta, true);
    lv_textarea_set_placeholder_text(network_port_ta, "Enter port");
    lv_obj_set_width(network_port_ta, 200);
    lv_obj_align_to(network_port_ta, port_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    // 绑定软键盘事件
    lv_obj_add_event_cb(network_port_ta, login_textarea_cb, LV_EVENT_ALL, keyboard);

    // === 连接按钮 ===
    lv_obj_t * connect_btn = lv_button_create(screen);
    lv_obj_set_size(connect_btn, 100, 40);
    lv_obj_align(connect_btn, LV_ALIGN_CENTER, 0, 80);
    lv_obj_add_event_cb(connect_btn, connect_to_server_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_label = lv_label_create(connect_btn);
    lv_label_set_text(btn_label, "Connect");
    lv_obj_center(btn_label);
    
    // === 返回按钮 ===
    lv_obj_t * back_btn = lv_button_create(screen);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_event_cb(back_btn, return_to_login_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
}
void connect_to_server_cb(lv_event_t * e)
{
    const char *ip = lv_textarea_get_text(network_ip_ta);
    const char *port = lv_textarea_get_text(network_port_ta);

    if (strlen(ip) == 0 || strlen(port) == 0) {
        printf("IP or port is empty\n");
        return;
    }

    // 启动客户端
    printf("Connecting to %s:%s\n", ip, port);
    if (start_client(ip, port) != 0) {
        printf("Failed to start client\n");
        return;
    }

    // 检查连接是否成功
    if (sockfd < 0) {
        printf("Socket connection failed\n");
        return;
    }

    // 先切换到游戏界面，确保global_canvas已初始化
    lv_obj_clean(lv_screen_active());
    main_interface();

    // 再启动网络线程
    network_running = true;
    printf("Creating network thread\n");
    
    if (pthread_create(&network_thread, NULL, network_thread_func, NULL) != 0) {
        perror("Failed to create network thread");
        close(sockfd);
        sockfd = -1;
        network_running = false;
        return;
    }
    pthread_detach(network_thread);

    // 设置为网络对战模式
    is_network_game = true;
    // 初始化玩家为BLACK，等待服务器分配
    current_player = BLACK;
    is_my_turn = false;
    printf("Connected to server, waiting for game to start...\n");
}
// =============== AI相关函数 ===============
int evaluate_position(int x, int y) {
    int my = ai_player;
    int opp = (my == BLACK) ? WHITE : BLACK;
    int score = 0;

    const int dx[] = {1, 0, 1, 1};
    const int dy[] = {0, 1, 1, -1};

    for (int i = 0; i < 4; i++) {
        int my_count = 0, opp_count = 0, open_ends = 0;

        // 正向
        for (int step = 1; step <= 4; step++) {
            int nx = x + dx[i] * step;
            int ny = y + dy[i] * step;
            if (nx < 0 || nx >= 15 || ny < 0 || ny >= 15) break;
            if (goban_board[ny][nx] == my) my_count++;
            else if (goban_board[ny][nx] == opp) { opp_count = 1; break; }
            else { open_ends++; break; }
        }

        // 反向
        for (int step = 1; step <= 4; step++) {
            int nx = x - dx[i] * step;
            int ny = y - dy[i] * step;
            if (nx < 0 || nx >= 15 || ny < 0 || ny >= 15) break;
            if (goban_board[ny][nx] == my) my_count++;
            else if (goban_board[ny][nx] == opp) { opp_count = 1; break; }
            else { open_ends++; break; }
        }

        if (opp_count == 0) {
            if (my_count >= 4) score += 10000;
            else if (my_count == 3 && open_ends >= 2) score += 1000;
            else if (my_count == 2 && open_ends >= 2) score += 100;
            else score += 10;
        }
    }

    return score;
}

// 替换 ai_make_move 函数，使用test.c的算法
void ai_make_move(lv_timer_t * timer) {
    if (!ai_mode_enabled || current_player != ai_player) {
        lv_timer_del(timer);
        return;
    }

    int computerX, computerY;
    
    // 将当前棋盘状态复制到test算法的棋盘中
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (goban_board[i][j] == EMPTY) {
                test_board[i][j] = EMPTY_TEST;
            } else if (goban_board[i][j] == BLACK) {
                test_board[i][j] = PLAYER_TEST; // 玩家是黑棋
            } else {
                test_board[i][j] = COMPUTER_TEST; // AI是白棋
            }
        }
    }
    emptyCount_test = 0;
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (test_board[i][j] == EMPTY_TEST) {
                emptyCount_test++;
            }
        }
    }
    
    // 调用test.c的AI算法
    computerMove_test(&computerX, &computerY, current_test_difficulty);
    
    // 将结果放置在棋盘上
    if (computerX >= 0 && computerX < SIZE && computerY >= 0 && computerY < SIZE) {
        place_stone_on_canvas(global_canvas, computerX, computerY, ai_player);
    }

    lv_timer_del(timer);
}

lv_obj_t* ai_mode_create_button(lv_obj_t* parent) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "AI Mode");
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, ai_mode_enable_cb, LV_EVENT_CLICKED, NULL);

    return btn;
}

// 修复 ai_mode_enable_cb 函数，修改难度选择以适配test.c的难度级别
void ai_mode_enable_cb(lv_event_t* e) {
    if (current_msgbox != NULL) {
        lv_obj_del(current_msgbox);
    }
    
    lv_obj_t* mbox = lv_obj_create(lv_screen_active());
    lv_obj_set_size(mbox, 200, 220);  // 稍微增加高度
    lv_obj_set_style_bg_color(mbox, lv_color_white(), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_radius(mbox, 8, 0);
    lv_obj_center(mbox);

    lv_obj_t* title = lv_label_create(mbox);
    lv_label_set_text(title, "AI Difficulty");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t* text = lv_label_create(mbox);
    lv_label_set_text(text, "Select AI Difficulty:");
    lv_obj_align_to(text, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t* btn_container = lv_obj_create(mbox);
    lv_obj_remove_style_all(btn_container);
    lv_obj_set_size(btn_container, 180, 120);  // 增加高度
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(btn_container, text, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    // 创建 Easy 按钮 (对应 EASY_TEST)
    lv_obj_t* btn_easy = lv_btn_create(btn_container);
    lv_obj_set_size(btn_easy, 100, 30);
    lv_obj_t* btn_label_easy = lv_label_create(btn_easy);
    lv_label_set_text(btn_label_easy, "Easy");
    lv_obj_center(btn_label_easy);
    lv_obj_add_event_cb(btn_easy, ai_difficulty_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)EASY_TEST);

    // 创建 Medium 按钮 (对应 MEDIUM_TEST)
    lv_obj_t* btn_medium = lv_btn_create(btn_container);
    lv_obj_set_size(btn_medium, 100, 30);
    lv_obj_t* btn_label_medium = lv_label_create(btn_medium);
    lv_label_set_text(btn_label_medium, "Medium");
    lv_obj_center(btn_label_medium);
    lv_obj_add_event_cb(btn_medium, ai_difficulty_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)MEDIUM_TEST);

    // 创建 Hard 按钮 (对应 HARD_TEST)
    lv_obj_t* btn_hard = lv_btn_create(btn_container);
    lv_obj_set_size(btn_hard, 100, 30);
    lv_obj_t* btn_label_hard = lv_label_create(btn_hard);
    lv_label_set_text(btn_label_hard, "Hard");
    lv_obj_center(btn_label_hard);
    lv_obj_add_event_cb(btn_hard, ai_difficulty_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)HARD_TEST);
    
    current_msgbox = mbox;
}

// 修改 ai_difficulty_btn_cb 函数以适配test.c的难度级别
void ai_difficulty_btn_cb(lv_event_t* e) {
    int selected_difficulty = (int)(intptr_t)lv_event_get_user_data(e);
    current_test_difficulty = selected_difficulty;
    ai_mode_enabled = true;
    ai_player = WHITE;
    current_player = BLACK;

    lv_obj_del(current_msgbox);
    current_msgbox = NULL;
    
    LV_LOG_USER("AI difficulty set to: %d", current_test_difficulty);
}

// 修改 process_network_message 函数中的所有中文文本
void process_network_message() {
    if (network_message_pending) {
        // 临时存储消息并重置标志
        char temp_message[1024];
        strncpy(temp_message, network_message, sizeof(temp_message) - 1);
        temp_message[sizeof(temp_message) - 1] = '\0';
        network_message_pending = 0;
        
        // 处理不同类型的网络消息
        if (strcmp(temp_message, "DISCONNECTED") == 0) {
            // 处理服务器断开连接
            if (!game_over) {
                if (status_label) {
                    lv_label_set_text(status_label, "Opponent disconnected");
                }
            }
            // 停止网络运行
            network_running = false;
            is_network_game = false;
            is_my_turn = false;
        }
        else if (strncmp(temp_message, "PLAYER", 6) == 0) {
            // 处理玩家分配消息
            if (strstr(temp_message, "BLACK")) {
                printf("Assigned as BLACK player\n");
                current_player = BLACK;
                if (status_label) {
                    lv_label_set_text(status_label, "You are Black (first)");
                }
            } else if (strstr(temp_message, "WHITE")) {
                printf("Assigned as WHITE player\n");
                current_player = WHITE;
                if (status_label) {
                    lv_label_set_text(status_label, "You are White (second)");
                }
            }
        } 
        else if (strcmp(temp_message, "START") == 0) {
            // 处理游戏开始消息
            printf("Game started!\n");
            is_my_turn = (current_player == BLACK);
            waiting_for_opponent = !is_my_turn;
            
            // 更新状态显示
            if (status_label) {
                if (is_my_turn) {
                    lv_label_set_text(status_label, "Your turn");
                } else {
                    lv_label_set_text(status_label, "Waiting for opponent...");
                }
            }
        } 
        else if (strncmp(temp_message, "MOVE", 4) == 0) {
            // 处理对手落子消息
            int row = atoi(temp_message + 5);
            int col = atoi(temp_message + 8);
            printf("Opponent moved to (%d, %d)\n", row, col);
            
            if (global_canvas) {
                pthread_mutex_lock(&network_mutex);
                int opponent = (current_player == BLACK) ? WHITE : BLACK;
                place_stone_on_canvas_remote(global_canvas, row, col, opponent);
                pthread_mutex_unlock(&network_mutex);
                
                // 切换回合
                is_my_turn = 1;
                waiting_for_opponent = 0;
                if (status_label) {
                    lv_label_set_text(status_label, "Your turn");
                }
            } else {
                printf("Warning: global_canvas is NULL, cannot place stone\n");
            }
        } 
        else if (strcmp(temp_message, "FULL") == 0) {
            // 处理服务器已满消息
            printf("Server is full, cannot join game\n");
            network_running = false;
            is_network_game = false;
            if (status_label) {
                lv_label_set_text(status_label, "Server is full");
            }
        }
    }
}

// 修改 network_thread_func 函数
void *network_thread_func(void *arg) {
    char buffer[1024];
    
    if (sockfd < 0) {
        printf("Invalid socket fd\n");
        network_running = false;
        is_network_game = false;
        
        // 通知主线程连接失败
        strncpy(network_message, "DISCONNECTED", sizeof(network_message) - 1);
        network_message_pending = 1;
        return NULL;
    }
    
    printf("Network thread started\n");
    
    while (network_running) {
        fd_set read_fds;
        struct timeval timeout;
        
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (network_running) {
                perror("select error");
            }
            break;
        }
        
        if (activity == 0) {
            continue;
        }
        
        if (FD_ISSET(sockfd, &read_fds)) {
            int read_size = read(sockfd, buffer, sizeof(buffer) - 1);
            if (read_size <= 0) {
                if (read_size == 0) {
                    printf("Server disconnected\n");
                } else {
                    perror("Receive failed");
                }
                
                // 设置断开连接消息，让主线程处理UI更新
                strncpy(network_message, "DISCONNECTED", sizeof(network_message) - 1);
                network_message_pending = 1;
                break;
            }
            
            buffer[read_size] = '\0';
            printf("Received from server: %s\n", buffer);
            
            // 将消息复制到全局变量，让主线程处理
            strncpy(network_message, buffer, sizeof(network_message) - 1);
            network_message[sizeof(network_message) - 1] = '\0';
            network_message_pending = 1;
        }
    }
    
    // 确保网络状态被正确设置
    network_running = false;
    printf("Network thread exiting\n");
    return NULL;
}