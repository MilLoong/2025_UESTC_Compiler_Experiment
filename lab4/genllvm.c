#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "genllvm.h"

/*
    输出缓冲区
    使用缓冲区收集所有LLVM IR输出
    支持标签回填(backpatch): 先输出占位符, 后续填入真实标签号
*/

#define BUFFER_SIZE 120000  // 缓冲区大小 (不能太大, 否则 icoding 会报错)
char* output_buffer = NULL;
size_t buffer_pos = 0; // 当前写入位置
size_t buffer_capacity = 0; // 缓冲区容量

// 初始化输出缓冲区
// 在代码生成开始时调用一次
void init_buffer() {
    if (output_buffer == NULL) {
        buffer_capacity = BUFFER_SIZE;
        output_buffer = (char*)malloc(buffer_capacity);
        if (output_buffer == NULL) {
            fprintf(stderr, "Failed to allocate output buffer\n");
            exit(1);
        }
    }
    buffer_pos = 0;
    output_buffer[0] = '\0';
}

// 确保缓冲区有足够空间
// 参数 needed: 需要的额外空间大小
void ensure_buffer_space(size_t needed) {
    if (buffer_pos + needed >= buffer_capacity) {
        buffer_capacity *= 2;
        output_buffer = (char*)realloc(output_buffer, buffer_capacity);
        if (output_buffer == NULL) {
            fprintf(stderr, "Failed to reallocate output buffer\n");
            exit(1);
        }
    }
}

// 格式化输出到缓冲区 (类似printf)
// 参数 format: 格式化字符串
// 参数 ...: 可变参数
void buf_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    int needed = vsnprintf(output_buffer + buffer_pos, 
                          buffer_capacity - buffer_pos, 
                          format, args);
    va_end(args);
    
    if (needed < 0) {
        fprintf(stderr, "vsnprintf error\n");
        exit(1);
    }
    
    // 如果空间不够, 扩容后重新写入
    if ((size_t)needed >= buffer_capacity - buffer_pos) {
        ensure_buffer_space(needed + 1000);
        va_start(args, format);
        needed = vsnprintf(output_buffer + buffer_pos, 
                          buffer_capacity - buffer_pos, 
                          format, args);
        va_end(args);
    }
    
    buffer_pos += needed;
}

// 将缓冲区内容输出到stdout
void flush_buffer() {
    printf("%s", output_buffer);
    fflush(stdout);
}

// 回填标签: 将缓冲区中的占位符替换为真实标签号
// 用于解决控制流语句中 先使用后定义 的标签问题
// 参数 placeholder: 占位符字符串 (如"{{T_1}}")
// 参数 value: 真实的标签数值
void backpatch_label(const char* placeholder, int value) {
    char val_str[32];
    sprintf(val_str, "%d", value);
    size_t val_len = strlen(val_str);
    size_t place_len = strlen(placeholder);
    
    // 遍历缓冲区, 查找并替换所有占位符
    for (size_t i = 0; i < buffer_pos; ) {
        if (buffer_pos - i >= place_len && 
            strncmp(output_buffer + i, placeholder, place_len) == 0) {
            
            // 将占位符替换为真实数值
            memcpy(output_buffer + i, val_str, val_len);
            
            // 如果真实数值比占位符短, 需要移动后续内容
            if (val_len < place_len) {
                memmove(output_buffer + i + val_len, 
                        output_buffer + i + place_len, 
                        buffer_pos - (i + place_len) + 1);
                buffer_pos -= (place_len - val_len);
            }
            i += val_len;
        } else {
            i++;
        }
    }
}

// 清理缓冲区, 释放内存
void cleanup_buffer() {
    if (output_buffer) {
        free(output_buffer);
        output_buffer = NULL;
    }
}

/*
    符号表数据结构
    用于管理变量、常量和数组信息
*/

#define ERROR_llvm_CONST_EXPR -1  // 常量表达式求值失败的错误码


// 键值对: 存储变量名->值 或 变量名->槽位号 的映射
struct KeyValuePair {
    char* key; // 变量名
    int value; // 对应的值或槽位号
};

// Map: 简单的哈希表实现
// 用于存储全局/局部的变量和常量信息
struct Map {
    struct KeyValuePair* data;
    size_t size;
};

// 数组信息: 记录数组的各维度大小
struct ArrayInfo {
    char* key; // 数组名
    int* dims; // 各维度大小数组
    int dim_count; // 维度数量
};

// 数组映射表
struct ArrayMap {
    struct ArrayInfo* data;
    size_t size;
};

// 待分配的alloca指令
// 用于实现变量提升: 先收集所有局部变量, 统一在函数入口处分配
struct PendingAlloca {
    int slot; // 槽位号
    struct PendingAlloca* next; // 链表的下一个节点
};

/*
    全局变量
*/

// 前向声明: 表达式求值 (全局)
int evalGlobalConstExpr(ast* pointer);
int evalGlobalAddExpr(ast* pointer);
int evalGlobalMulExpr(ast* pointer);
int evalGlobalUnaryExpr(ast* pointer);

// 前向声明: 表达式求值 (局部)
int evalLocalConstExpr(ast* pointer); 

// 前向声明: 表达式类型检查
int isRuntimeExpr(ast* pointer);
int isRuntimeMulExpr(ast* pointer);
int isRuntimeUnaryExpr(ast* pointer);

// 前向声明: 辅助函数
void enqueue_alloca(int slot);
void flush_pending_allocas(void);
void print_operand(ast* node);

// 前向声明: 表达式代码生成
int genArithmeticExpr(ast* pointer);
int genMulExpr(ast* pointer);
int genUnaryExpr(ast* pointer);
int genLogicExpr(ast* pointer);
int genEqualityExpr(ast* pointer);
int genLogicalAndExpr(ast* pointer);
int genLogicalOrExpr(ast* pointer);
int genCondition(ast* pointer);

// 前向声明: 条件跳转生成
void genCondWithJumpPlaceholder(ast* pointer, const char* true_label_ph, const char* false_label_ph);
void genCondWithJump(ast* pointer, int true_label, int false_label);

// 前向声明: 函数调用参数
int genFuncCallArgs(ast* pointer, char* buffer);

// 前向声明: 语句生成
int genStmt(ast* pointer);
void genIfElseChain(ast* pointer, const char* end_label);

// 前向声明: 全局声明生成
int genGlobalConstDecl(ast* pointer);
int genGlobalConstDef(ast* pointer);
int genGlobalVarDecl(ast* pointer);
int genGlobalVarDef(ast* pointer);

// 前向声明: 函数定义生成
int genFuncDef(ast* pointer);
int genFuncParams(ast* pointer, int* num);
int genFuncParam(ast* pointer, int* num);
int genBlock(ast* pointer);
int genBlock_A(ast* pointer);
int genBlockItem(ast* pointer);

// 前向声明: 局部声明生成
int genLocalConstDecl(ast* pointer);
int genLocalConstDef(ast* pointer);
int genLocalVarDecl(ast* pointer);
int genLocalVarDef(ast* pointer);

int func_exit_label = -1; // 函数退出标签 (-99表示使用占位符{{EXIT}})

// 符号表: 分为全局和局部, 每个又分为ID表和常量表
struct Map id_global; // 全局变量表 (变量名 -> 初始值)
struct Map id_local;  // 局部变量表 (变量名 -> 槽位号)
struct Map const_global; // 全局常量表 (常量名 -> 常量值)
struct Map const_local;  // 局部常量表 (常量名 -> 常量值)

struct ArrayMap array_global; // 数组信息表 (全局+局部)

// 待分配的alloca链表
struct PendingAlloca* pending_allocas_head = NULL;
struct PendingAlloca* pending_allocas_tail = NULL;

/*
    符号表操作函数
*/

// 初始化Map
void initializeMap(struct Map* map) {
    map->data = NULL;
    map->size = 0;
}

// 向Map中插入键值对
// 参数 map: 目标Map
// 参数 key: 键 (变量名)
// 参数 value: 值 (数值或槽位号)
void insertKeyValuePair(struct Map* map, const char* key, int value) {
    map->data = realloc(map->data, (map->size + 1) * sizeof(struct KeyValuePair));
    map->data[map->size].key = strdup(key);
    map->data[map->size].value = value;
    map->size++;
}

// 清理Map, 释放所有内存
void cleanupMap(struct Map* map) {
    for (size_t i = 0; i < map->size; ++i) {
        free(map->data[i].key);
    }
    free(map->data);
    map->data = NULL;
    map->size = 0;
}

// 从Map中查找键, 返回索引
// 返回值: 找到返回索引, 未找到返回-1
int getValueFromMap(struct Map* map, const char* key) {
    for (size_t i = 0; i < map->size; ++i) {
        if (strcmp(map->data[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

// 从Map中获取值, 未找到返回错误码
// 参数 error_code: 未找到时的返回值
int getVal(struct Map* map, const char* key, int error_code) {
    int idx = getValueFromMap(map, key);
    if (idx != -1) return map->data[idx].value;
    return error_code;
}

// 初始化数组映射表
void initializeArrayMap(struct ArrayMap* map) {
    map->data = NULL;
    map->size = 0;
}

// 插入数组信息
// 参数 key 数组名
// 参数 dims 维度数组
// 参数 dim_count 维度数量
void insertArrayInfo(struct ArrayMap* map, const char* key, int* dims, int dim_count) {
    map->data = realloc(map->data, (map->size + 1) * sizeof(struct ArrayInfo));
    map->data[map->size].key = strdup(key);
    map->data[map->size].dim_count = dim_count;
    map->data[map->size].dims = (int*)malloc(sizeof(int) * dim_count);
    memcpy(map->data[map->size].dims, dims, sizeof(int) * dim_count);
    map->size++;
}

// 获取数组信息
// 返回值: 找到返回ArrayInfo指针, 未找到返回NULL
struct ArrayInfo* getArrayInfo(struct ArrayMap* map, const char* key) {
    for (size_t i = 0; i < map->size; ++i) {
        if (strcmp(map->data[i].key, key) == 0) {
            return &map->data[i];
        }
    }
    return NULL;
}

// 清理数组映射表
void cleanupArrayMap(struct ArrayMap* map) {
    for (size_t i = 0; i < map->size; ++i) {
        free(map->data[i].key);
        free(map->data[i].dims);
    }
    free(map->data);
    map->data = NULL;
    map->size = 0;
}

/*
    数组处理辅助函数
*/

// 收集数组各维度大小
// 参数 node: 维度链表头节点
// 参数 dims: 输出: 维度数组 (动态分配)
// 参数 dim_count: 输出: 维度数量
void collect_const_dims(ast* node, int** dims, int* dim_count) {
    ast* cursor = node;
    while (cursor != NULL) {
        (*dim_count)++;
        *dims = realloc(*dims, sizeof(int) * (*dim_count));
        (*dims)[(*dim_count) - 1] = evalGlobalConstExpr(cursor->right);
        cursor = cursor->next;
    }
}

// 将数组初始化列表展平成一维数组
// 处理嵌套的{}初始化, 如: {{1,2},{3,4}} -> [1,2,3,4]
// 参数 node: 初始化列表AST节点
// 参数 values: 输出: 展平后的值数组
// 参数 total: 数组总元素个数
// 参数 index: 当前填充位置
void flatten_const_init(ast* node, int* values, int total, int* index) {
    if (node == NULL || *index >= total) {
        return;
    }
    
    if (strcmp(node->node_type, "CONST_INIT") != 0) {
        // 叶子节点: 直接存储值
        values[(*index)++] = evalLocalConstExpr(node);
        return;
    }
    
    if (strcmp(node->value_string, "{}") == 0) {
        // 嵌套的初始化列表
        if (node->right != NULL) {
            flatten_const_init(node->right, values, total, index);
        }
        ast* tail = node->next;
        while (tail != NULL && *index < total) {
            flatten_const_init(tail->right, values, total, index);
            tail = tail->next;
        }
    } else {
        // 单个值
        values[(*index)++] = evalLocalConstExpr(node->right);
        if (node->next != NULL) {
            flatten_const_init(node->next, values, total, index);
        }
    }
}

// 递归构建数组类型字符串 (内部函数)
// 如: dims=[2,3] -> "[2 x [3 x i32]]"
void build_array_type_string_internal(int* dims, int dim_count, int level, char* buffer, size_t len) {
    if (level == dim_count - 1) {
        // 最内层: 直接输出 [size x i32]
        snprintf(buffer + strlen(buffer), len - strlen(buffer), "[%d x i32]", dims[level]);
        return;
    }
    // 递归构建: [size x [内层类型]]
    snprintf(buffer + strlen(buffer), len - strlen(buffer), "[%d x ", dims[level]);
    build_array_type_string_internal(dims, dim_count, level + 1, buffer, len);
    snprintf(buffer + strlen(buffer), len - strlen(buffer), "]");
}

// 构建LLVM数组类型字符串
// 参数 dims: 维度数组
// 参数 dim_count: 维度数量
// 参数 buffer: 输出缓冲区
// 参数 len: 缓冲区大小
void build_array_type_string(int* dims, int dim_count, char* buffer, size_t len) {
    buffer[0] = '\0';
    build_array_type_string_internal(dims, dim_count, 0, buffer, len);
}

// 递归打印数组字面量 (内部函数)
// 如: values=[1,2,3,4], dims=[2,2] -> "[i32 1, i32 2], [i32 3, i32 4]"
void print_array_literal_internal(int* values, int level, int dim_count, int* dims, int* index) {
    buf_printf("[");
    for (int i = 0; i < dims[level]; ++i) {
        if (i > 0) {
            buf_printf(", ");
        }
        if (level == dim_count - 1) {
            // 最内层: 直接输出元素
            buf_printf("i32 %d", values[(*index)++]);
        } else {
            // 递归输出子数组
            print_array_literal_internal(values, level + 1, dim_count, dims, index);
        }
    }
    buf_printf("]");
}

// 打印数组字面量到缓冲区
void print_array_literal(int* values, int level, int dim_count, int* dims, int* index) {
    print_array_literal_internal(values, level, dim_count, dims, index);
}

/*
    局部变量管理
*/

static char* func_param_names[256]; // 函数参数名列表
int scout; // 全局寄存器/标签计数器

// 创建局部变量槽位
// 参数 name: 变量名 (NULL表示匿名临时变量)
// 返回值: 分配的槽位号
int create_local_slot(const char* name) {
    int slot = scout++;
    enqueue_alloca(slot); // 加入待分配队列
    if (name != NULL) {
        char* temp = strdup(name);
        insertKeyValuePair(&id_local, temp, slot);
    }
    return slot;
}

// 获取局部变量的槽位号
// 返回值: 找到返回槽位号, 未找到返回-1
int get_local_slot(const char* name) {
    int index = getValueFromMap(&id_local, name);
    if (index == -1) {
        return -1;
    }
    return id_local.data[index].value;
}

// 将alloca指令加入待分配队列
// 实现变量提升: 所有局部变量的alloca都在函数入口统一生成
void enqueue_alloca(int slot) {
    struct PendingAlloca* node = (struct PendingAlloca*)malloc(sizeof(struct PendingAlloca));
    node->slot = slot;
    node->next = NULL;
    if (pending_allocas_tail == NULL) {
        pending_allocas_head = pending_allocas_tail = node;
    } else {
        pending_allocas_tail->next = node;
        pending_allocas_tail = node;
    }
}

// 输出所有待分配的alloca指令
// 在函数体开始处调用, 确保所有局部变量在entry block分配
void flush_pending_allocas(void) {
    struct PendingAlloca* cursor = pending_allocas_head;
    while (cursor != NULL) {
        buf_printf("  %%%d = alloca i32, align 4\n", cursor->slot);
        struct PendingAlloca* temp = cursor;
        cursor = cursor->next;
        free(temp);
    }
    pending_allocas_head = pending_allocas_tail = NULL;
}

/*
    变量提升(Hoisting)
    在生成函数体代码前, 先遍历AST收集所有局部变量声明
    为每个变量预分配槽位, 避免代码生成时才分配导致的顺序问题
*/

// 前向声明
void hoist_block(ast* pointer);
void hoist_block_a(ast* pointer);
void hoist_block_item(ast* pointer);
void hoist_const_decl(ast* pointer);
void hoist_const_def(ast* pointer);
void hoist_var_decl(ast* pointer);
void hoist_var_def(ast* pointer);
void hoist_stmt(ast* pointer);

// 检查字符串是否匹配特定token
int is_token(const char* value, const char* token) {
    if (value == NULL || token == NULL) return 0;
    return strcmp(value, token) == 0;
}

// 提升代码块中的变量声明
void hoist_block(ast* pointer) {
    if (pointer == NULL) return;
    if (pointer->right != NULL) hoist_block_a(pointer->right);
}

// 提升块项链表
void hoist_block_a(ast* pointer) {
    if (pointer == NULL) return;
    hoist_block_item(pointer->right);
    if (pointer->next != NULL) hoist_block_a(pointer->next);
}

// 提升单个块项 (声明或语句)
void hoist_block_item(ast* pointer) {
    if (pointer == NULL) return;
    if (strcmp(pointer->node_type, "CONST_DECL") == 0) 
        hoist_const_decl(pointer);
    else if (strcmp(pointer->node_type, "VAR_DECL") == 0) 
        hoist_var_decl(pointer);
    else if (strcmp(pointer->node_type, "STMT") == 0) 
        hoist_stmt(pointer);
    else if (strcmp(pointer->node_type, "BLOCK") == 0) 
        hoist_block(pointer);
}

// 提升常量声明链表
void hoist_const_decl(ast* pointer) {
    if (pointer == NULL) return;
    hoist_const_def(pointer->right);
    if (pointer->next != NULL) hoist_const_decl(pointer->next);
}

// 提升单个常量定义
// 注: 只为标量常量预分配槽位, 数组常量在生成时处理
void hoist_const_def(ast* pointer) {
    if (pointer == NULL) return;
    if (pointer->left == NULL) {
        // 标量常量
        if (get_local_slot(pointer->value_string) == -1) {
            create_local_slot(pointer->value_string);
        }
    }
    // 数组常量的alloca在genLocalConstDef中处理
}

// 提升变量声明链表
void hoist_var_decl(ast* pointer) {
    if (pointer == NULL) return;
    hoist_var_def(pointer->right);
    if (pointer->next != NULL) hoist_var_decl(pointer->next);
}

// 提升单个变量定义
// 注: 只为标量变量预分配槽位
void hoist_var_def(ast* pointer) {
    if (pointer == NULL) return;
    if (pointer->left == NULL) {
        // 标量变量
        if (get_local_slot(pointer->value_string) == -1) {
            create_local_slot(pointer->value_string);
        }
    }
}

// 提升语句中的嵌套块
// 处理if/while等控制流语句中的嵌套作用域
void hoist_stmt(ast* pointer) {
    if (pointer == NULL) return;
    
    if (strcmp(pointer->value_string, "") == 0) {
        // 空语句可能包含块
        if (pointer->left != NULL && strcmp(pointer->left->node_type, "BLOCK") == 0) {
            hoist_block(pointer->left);
        }
    } else if (is_token(pointer->value_string, "if") || is_token(pointer->value_string, "IF")) {
        // if语句: 提升then和else分支
        if (pointer->right != NULL) hoist_stmt(pointer->right);
        if (pointer->extend != NULL) hoist_stmt(pointer->extend);
    } else if (is_token(pointer->value_string, "while") || is_token(pointer->value_string, "WHILE")) {
        // while语句: 提升循环体
        if (pointer->right != NULL) hoist_stmt(pointer->right);
    } else if (strcmp(pointer->value_string, "Block") == 0) {
        if (pointer->left != NULL) hoist_block(pointer->left);
    }
}

/*
    辅助函数
*/

// 生成新的标签 (返回字符串形式)
char* generate_label() {
    char* label = (char*)malloc(20);
    sprintf(label, "%d", scout++);
    return label;
}

// 将i1类型扩展为i32类型
// LLVM中布尔值是i1, 但C语言中整数和布尔可以互转, 需要扩展
int zext_i1_to_i32(int reg) {
    int res = scout++;
    buf_printf("  %%%d = zext i1 %%%d to i32\n", res, reg);
    return res;
}

// 打印操作数 (寄存器或立即数)
// 如果是运行时表达式, 打印寄存器号；如果是常量, 直接打印数值
void print_operand(ast* node); // 前向声明

// 检查节点是否以return结尾
// 用于判断是否需要在块末尾补充返回指令
int ends_with_return(ast* node) {
    if (!node) return 0;
    
    if (strcmp(node->node_type, "STMT") == 0) {
        if (strcmp(node->value_string, "return") == 0) {
            return 1;
        }
        if (strcmp(node->value_string, "if") == 0) {
            // if语句: 只有when then和else都以return结尾时才返回1
            int then_returns = ends_with_return(node->right);
            int else_returns = node->extend ? ends_with_return(node->extend) : 0;
            return then_returns && else_returns;
        }
        if (strcmp(node->value_string, "") == 0 && node->left) {
            // 检查包装节点
            if (strcmp(node->left->node_type, "BLOCK") == 0) {
                return ends_with_return(node->left);
            }
        }
    } else if (strcmp(node->node_type, "BLOCK") == 0) {
        // 检查块的最后一条语句
        if (node->right) {
            ast* item = node->right;
            while (item->next) item = item->next; // 找到链表最后一个节点
            if (item->right) {
                return ends_with_return(item->right);
            }
        }
    }
    
    return 0;
}

// 检查缓冲区最后一条指令是否是终结指令(br/ret)
// 用于避免重复生成返回指令
int string_ends_with_terminator(const char* str) {
    if (!str || strlen(str) == 0) return 0;
    
    // 找到最后一个非空内容
    const char* ptr = str + strlen(str) - 1;
    while (ptr > str && (*ptr == '\n' || *ptr == ' ')) ptr--;
    
    // 找到行的开始
    while (ptr > str && *ptr != '\n') ptr--;
    if (*ptr == '\n') ptr++;
    
    if (strncmp(ptr, "  br", 4) == 0) return 1;
    if (strncmp(ptr, "  ret", 5) == 0) return 1;
    return 0;
}

// 统计AST中return语句的数量
// 用于判断函数是简单函数还是复杂函数
int count_return(ast* node) {
    if (!node) return 0;
    int cnt = 0;
    if (strcmp(node->node_type, "STMT") == 0 && strcmp(node->value_string, "return") == 0)
        cnt++;
    if (node->left) cnt += count_return(node->left);
    if (node->right) cnt += count_return(node->right);
    if (node->extend) cnt += count_return(node->extend);
    if (node->next) cnt += count_return(node->next);
    return cnt;
}

/*
    表达式类型检查
    判断表达式是编译时常量还是运行时计算值
    返回0表示常量 (可以在编译时求值), 返回1表示需要生成代码
*/

// 前向声明
int isRuntimeExpr(ast* pointer);
int isRuntimeMulExpr(ast* pointer);
int isRuntimeUnaryExpr(ast* pointer);

// 检查一元表达式是否需要运行时计算
// 返回值: 0=编译时常量, 1=运行时表达式
int isRuntimeUnaryExpr(ast* pointer) {
    if (!pointer) return 0;

    // 处理透传节点 (语法树中的包装层)
    if (strcmp(pointer->node_type, "REL_EXP") == 0 || 
        strcmp(pointer->node_type, "EQ_EXP") == 0 ||
        strcmp(pointer->node_type, "L_AND_EXP") == 0 || 
        strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            // 无操作符: 只是包装层, 继续检查内层
            return isRuntimeUnaryExpr(pointer->left);
        }
        // 有操作符: 是运行时比较操作
        return 1;
    }

    if (strcmp(pointer->node_type, "EXP_WITH_UNARY_OP") == 0) {
        return isRuntimeUnaryExpr(pointer->right);
    } else if (strcmp(pointer->node_type, "FUNC_CALL_ID") == 0) {
        return 1; // 函数调用必然是运行时
    } else if (strcmp(pointer->node_type, "ID_WITH_SUBSCRIPT") == 0) {
        return 1; // 数组访问需要load指令
    } else {
        if (strcmp(pointer->node_type, "EXPR") == 0) 
            return isRuntimeExpr(pointer);
        else if (strcmp(pointer->node_type, "EXPR_MUL") == 0) 
            return isRuntimeMulExpr(pointer);
        else if (strcmp(pointer->node_type, "IDENTIFIER") == 0) {
            // 检查是否是常量
            if (getValueFromMap(&const_local, pointer->value_string) != -1) return 0;
            if (getValueFromMap(&const_global, pointer->value_string) != -1) return 0;
            return 1; // 是变量
        } else if (strcmp(pointer->node_type, "DECIMAL") == 0 || 
                   strcmp(pointer->node_type, "OCTAL") == 0 || 
                   strcmp(pointer->node_type, "HEXADECIMAL") == 0) {
            return 0; // 字面量是常量
        }
    }
    return 0;
}

// 检查加减法表达式是否需要运行时计算
int isRuntimeExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR") == 0) 
        return isRuntimeExpr(pointer->left) + isRuntimeMulExpr(pointer->right);
    return isRuntimeMulExpr(pointer);
}

// 检查乘除模表达式是否需要运行时计算
int isRuntimeMulExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR_MUL") == 0)
        return isRuntimeMulExpr(pointer->left) + isRuntimeUnaryExpr(pointer->right);
    return isRuntimeUnaryExpr(pointer);
}

// 打印操作数 (实现)
// 根据表达式类型选择打印寄存器号或立即数
void print_operand(ast* node) {
    if (isRuntimeExpr(node)) {
        // 运行时表达式: 打印寄存器
        buf_printf("%%%d", genArithmeticExpr(node));
    } else {
        // 编译时常量: 直接打印数值
        buf_printf("%d", evalLocalConstExpr(node));
    }
}

/*
    全局常量表达式求值
    在编译时计算常量表达式的值 (用于全局常量/变量初始化)
*/

// 求值全局常量表达式
// 返回值: 常量值, 失败返回ERROR_llvm_CONST_EXPR
int evalGlobalConstExpr(ast* pointer) {
    if (!pointer) return ERROR_llvm_CONST_EXPR;
    
    // 基础数值字面量
    if (strcmp(pointer->node_type, "DECIMAL") == 0) return pointer->value_int;
    if (strcmp(pointer->node_type, "OCTAL") == 0) return pointer->value_int;
    if (strcmp(pointer->node_type, "HEXADECIMAL") == 0) return pointer->value_int;
    
    // 标识符引用 (全局常量)
    if (strcmp(pointer->node_type, "IDENTIFIER") == 0) {
        return getVal(&const_global, pointer->value_string, 0);
    }
    
    // 二元运算
    if (strcmp(pointer->node_type, "EXPR") == 0 || strcmp(pointer->node_type, "EXPR_MUL") == 0) {
        int l = evalGlobalConstExpr(pointer->left);
        int r = evalGlobalConstExpr(pointer->right);
        switch (pointer->value_int) {
            case '+': return l + r;
            case '-': return l - r;
            case '*': return l * r;
            case '/': return r ? l / r : 0;
            case '%': return r ? l % r : 0;
        }
    }
    
    // 一元运算
    if (strcmp(pointer->node_type, "EXP_WITH_UNARY_OP") == 0) {
        int r = evalGlobalConstExpr(pointer->right);
        switch (pointer->left->value_int) {
            case '+': return r;
            case '-': return -r;
            case '!': return !r;
        }
    }
    
    // 处理透传节点 (比较 或 逻辑表达式的包装层)
    if (strcmp(pointer->node_type, "REL_EXP") == 0 || 
        strcmp(pointer->node_type, "EQ_EXP") == 0 ||
        strcmp(pointer->node_type, "L_AND_EXP") == 0 || 
        strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            return evalGlobalConstExpr(pointer->left);
        }
    }

    return ERROR_llvm_CONST_EXPR;
}

// 求值局部常量表达式
// 与全局版本的区别: 可以引用局部常量
int evalLocalConstExpr(ast* pointer) {
    if (!pointer) return 0;
    
    if (strcmp(pointer->node_type, "DECIMAL") == 0) return pointer->value_int;
    if (strcmp(pointer->node_type, "OCTAL") == 0) return pointer->value_int;
    if (strcmp(pointer->node_type, "HEXADECIMAL") == 0) return pointer->value_int;
    
    // 标识符: 先查局部常量, 再查全局常量
    if (strcmp(pointer->node_type, "IDENTIFIER") == 0) {
        int idx = getValueFromMap(&const_local, pointer->value_string);
        if (idx != -1) return const_local.data[idx].value;
        idx = getValueFromMap(&const_global, pointer->value_string);
        if (idx != -1) return const_global.data[idx].value;
        return 0;
    }

    // 二元运算
    if (strcmp(pointer->node_type, "EXPR") == 0 || strcmp(pointer->node_type, "EXPR_MUL") == 0) {
        int l = evalLocalConstExpr(pointer->left);
        int r = evalLocalConstExpr(pointer->right);
        switch (pointer->value_int) {
            case '+': return l + r;
            case '-': return l - r;
            case '*': return l * r;
            case '/': return r ? l / r : 0;
            case '%': return r ? l % r : 0;
        }
    }
    
    // 一元运算
    if (strcmp(pointer->node_type, "EXP_WITH_UNARY_OP") == 0) {
        int r = evalLocalConstExpr(pointer->right);
        switch (pointer->left->value_int) {
            case '+': return r;
            case '-': return -r;
            case '!': return !r;
        }
    }

    // 处理透传节点
    if (strcmp(pointer->node_type, "REL_EXP") == 0 || 
        strcmp(pointer->node_type, "EQ_EXP") == 0 ||
        strcmp(pointer->node_type, "L_AND_EXP") == 0 || 
        strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            return evalLocalConstExpr(pointer->left);
        }
    }
    
    return 0;
}

/*
    全局变量 和 常量 代码生成
*/

// 生成全局常量声明链表
int genGlobalConstDecl(ast* pointer) {
    genGlobalConstDef(pointer->right);
    if (pointer->next != NULL) {
        genGlobalConstDecl(pointer->next);
    }
    return 0;
}

// 生成全局常量定义
// 输出类似: @const_name = constant i32 42, align 4
int genGlobalConstDef(ast* pointer) {
    if (pointer->left == NULL) {
        // 标量常量
        int value = evalGlobalConstExpr(pointer->right->right);
        buf_printf("@%s = constant i32 %d, align 4\n", pointer->value_string, value);
        char* temp = strdup(pointer->value_string);
        
        insertKeyValuePair(&id_global, temp, value);
        insertKeyValuePair(&const_global, temp, value); // 记录为常量
    } else {
        // 数组常量
        int* dims = NULL;
        int dim_count = 0;
        collect_const_dims(pointer->left, &dims, &dim_count);
        
        // 计算总元素数
        int total = 1;
        for (int i = 0; i < dim_count; ++i) {
            total *= dims[i];
        }
        
        // 展平初始化列表
        int* values = (int*)calloc(total, sizeof(int));
        int index = 0;
        flatten_const_init(pointer->right, values, total, &index);
        while (index < total) {
            values[index++] = 0; // 未初始化部分填0
        }
        
        // 构建类型字符串并输出
        char type_buffer[256];
        build_array_type_string(dims, dim_count, type_buffer, sizeof(type_buffer));
        buf_printf("@%s = constant %s ", pointer->value_string, type_buffer);
        
        int literal_index = 0;
        print_array_literal(values, 0, dim_count, dims, &literal_index);
        buf_printf(", align 16\n");
        
        insertArrayInfo(&array_global, pointer->value_string, dims, dim_count);
        free(values);
        free(dims);
    }
    return 0;
}

// 生成全局变量声明链表
int genGlobalVarDecl(ast* pointer) {
    genGlobalVarDef(pointer->right);
    if (pointer->next != NULL) {
        genGlobalVarDecl(pointer->next);
    }
    return 0;
}

// 生成全局变量定义
// 输出类似: @var_name = global i32 0, align 4
int genGlobalVarDef(ast* pointer) {
    // 处理数组
    if (pointer->left != NULL) {
        int* dims = NULL;
        int dim_count = 0;
        collect_const_dims(pointer->left, &dims, &dim_count);
        char type_buffer[256];
        build_array_type_string(dims, dim_count, type_buffer, sizeof(type_buffer));
        
        // 数组使用zeroinitializer (全0初始化)
        buf_printf("@%s = global %s zeroinitializer, align 16\n", pointer->value_string, type_buffer);
        
        insertArrayInfo(&array_global, pointer->value_string, dims, dim_count);
        if(dims) free(dims);
        return 0;
    }

    // 处理标量
    if (pointer->right == NULL) {
        // 无初始化: 默认为0
        buf_printf("@%s = global i32 0, align 4\n", pointer->value_string);
        char* temp = strdup(pointer->value_string);
        insertKeyValuePair(&id_global, temp, 0);
    } else {
        // 有初始化值
        int value = evalGlobalAddExpr(pointer->right->right);
        buf_printf("@%s = global i32 %d, align 4\n", pointer->value_string, value);
        char* temp = strdup(pointer->value_string);
        insertKeyValuePair(&id_global, temp, value);
    }
    return 0;
}

// 求值全局变量初始化表达式 (加减法)
int evalGlobalAddExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR") == 0) {
        if (pointer->value_int == '+') 
            return evalGlobalAddExpr(pointer->left) + evalGlobalMulExpr(pointer->right);
        else if (pointer->value_int == '-') 
            return evalGlobalAddExpr(pointer->left) - evalGlobalMulExpr(pointer->right);
    }
    return evalGlobalMulExpr(pointer);
}

// 求值全局变量初始化表达式 (乘除模)
int evalGlobalMulExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR_MUL") == 0) {
        if (pointer->value_int == '*') 
            return evalGlobalMulExpr(pointer->left) * evalGlobalUnaryExpr(pointer->right);
        else if (pointer->value_int == '/') 
            return evalGlobalMulExpr(pointer->left) / evalGlobalUnaryExpr(pointer->right);
        else if (pointer->value_int == '%') 
            return evalGlobalMulExpr(pointer->left) % evalGlobalUnaryExpr(pointer->right);
    }
    return evalGlobalUnaryExpr(pointer);
}

// 求值全局变量初始化表达式 (一元运算)
int evalGlobalUnaryExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXP_WITH_UNARY_OP") == 0) {
        switch (pointer->left->value_int) {
            case '+': return evalGlobalUnaryExpr(pointer->right);
            case '-': return -evalGlobalUnaryExpr(pointer->right);
            case '!': return evalGlobalUnaryExpr(pointer->right) == 0 ? 1 : 0;
        }
    } else if (strcmp(pointer->node_type, "IDENTIFIER") == 0) {
        // 优先查常量表
        if (getValueFromMap(&const_global, pointer->value_string) != -1)
             return getVal(&const_global, pointer->value_string, 0);
        // 再查ID表
        if (getValueFromMap(&id_global, pointer->value_string) != -1)
             return getVal(&id_global, pointer->value_string, 0);
        exit(-1);
    } else if (strcmp(pointer->node_type, "DECIMAL") == 0) {
        return pointer->value_int;
    }
    return 0;
}

/*
    函数代码生成
*/

// 前向声明
int genFuncDef(ast* pointer);
int genFuncParams(ast* pointer, int* num);
int genFuncParam(ast* pointer, int* num);
int genBlock(ast* pointer);
int genBlock_A(ast* pointer);
int genBlockItem(ast* pointer);
int genLocalConstDecl(ast* pointer);
int genLocalConstDef(ast* pointer);
int genLocalVarDecl(ast* pointer);
int genLocalVarDef(ast* pointer);

// 生成函数定义
// 处理: 
// - 函数签名
// - 参数处理
// - 变量提升
// - 函数体生成
// - 返回指令补充
int genFuncDef(ast* pointer) {
    // 输出函数签名
    if (strcmp(pointer->value_string, "int") == 0) {
        buf_printf("define i32 @%s(", pointer->value_string1);
    } else if (strcmp(pointer->value_string, "void") == 0) {
        buf_printf("define void @%s(", pointer->value_string1);
    }

    // 初始化局部符号表
    cleanupMap(&id_local);
    initializeMap(&id_local);
    initializeMap(&const_local);
    memset(func_param_names, 0, sizeof(func_param_names));

    // 生成参数列表
    int num = 0;
    if (pointer->left != NULL) {
        genFuncParams(pointer->left, &num);
    }
    buf_printf(") {\n");

    scout = num + 1; // 设置寄存器计数器起始值
    
    // 判断函数复杂度
    // 简单函数: 只有一个return语句, 无控制流
    // 复杂函数: 多个return或包含 if 或 while
    int ret_slot = -1;
    int is_complex = 0;
    int return_count = count_return(pointer->right);
    
    if (return_count > 1 || 
        (pointer->right && (strstr(pointer->right->node_type, "IF") || 
                           strstr(pointer->right->node_type, "WHILE")))) {
        is_complex = 1;
    }

    // 为复杂函数或main函数创建返回值槽位
    // 复杂函数需要统一的退出点来收集返回值
    if (is_complex && strcmp(pointer->value_string, "void") != 0) {
        ret_slot = create_local_slot("__ret");
    } else if (strcmp(pointer->value_string1, "main") == 0 && 
               strcmp(pointer->value_string, "void") != 0) {
        ret_slot = create_local_slot("__ret");
    }
    
    // 为参数创建槽位
    for (int i = 0; i < num; ++i) {
        if (func_param_names[i] == NULL) continue;
        create_local_slot(func_param_names[i]);
    }

    // 变量提升: 收集所有局部变量声明
    hoist_block(pointer->right);
    flush_pending_allocas(); // 输出所有alloca指令

    // main函数默认返回0
    if (ret_slot != -1 && strcmp(pointer->value_string1, "main") == 0) {
        buf_printf("  store i32 0, i32* %%%d, align 4\n", ret_slot);
    }

    // 将参数值存入对应槽位
    for (int i = 0; i < num; ++i) {
        if (func_param_names[i] == NULL) continue;
        int slot = get_local_slot(func_param_names[i]);
        buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", i, slot);
    }

    // 设置退出标签模式
    // -99表示使用占位符{{EXIT}}, 稍后回填
    if (is_complex && strcmp(pointer->value_string, "void") != 0) {
        func_exit_label = -99;
    } else {
        func_exit_label = -1;
    }

    // 生成函数体
    genBlock(pointer->right);

    // 处理函数结尾
    if (func_exit_label == -99) {
        // 复杂函数: 统一跳转到EXIT标签返回
        int real_exit = scout++;
        backpatch_label("{{EXIT}}", real_exit);
        
        buf_printf("\n%d:\n", real_exit);
        int ret_reg = scout++;
        buf_printf("  %%%d = load i32, i32* %%%d, align 4\n", ret_reg, ret_slot);
        buf_printf("  ret i32 %%%d\n", ret_reg);
        
    } else if (strcmp(pointer->value_string1, "main") == 0 && 
               strcmp(pointer->value_string, "void") != 0) {
        // 简单main函数: 检查是否需要补充return
        if (!string_ends_with_terminator(output_buffer)) {
            buf_printf("  ret i32 0\n");
        }
        buf_printf("}\n");
        
        flush_pending_allocas();
        cleanupMap(&id_local);
        cleanupMap(&const_local);
        return 0;
        
    } else if (strcmp(pointer->value_string, "void") == 0) {
        // void函数: 检查是否需要补充ret void
        if (!string_ends_with_terminator(output_buffer)) {
            buf_printf("  ret void\n");
        }
    }
    
    buf_printf("}\n");
    flush_pending_allocas();
    cleanupMap(&id_local);
    cleanupMap(&const_local);
    return 0;
}

// 生成函数参数列表
// 参数 num: 输出参数个数
int genFuncParams(ast* pointer, int* num) {
    if (pointer == NULL) return 0;
    if (*num > 0) buf_printf(", ");
    genFuncParam(pointer->right, num);
    (*num)++;
    if (pointer->next != NULL) genFuncParams(pointer->next, num);
    return 0;
}

// 生成单个函数参数
// 输出类似: i32 %0
int genFuncParam(ast* pointer, int* num) {
    int index = *num;
    buf_printf("i32 %%%d", index);
    if (index < (int)(sizeof(func_param_names) / sizeof(func_param_names[0]))) {
        func_param_names[index] = pointer->value_string1;
    }
    return 0;
}

// 生成代码块
int genBlock(ast* pointer) {
    if (pointer->right != NULL) genBlock_A(pointer->right);
    return 0;
}

// 生成代码块项链表
int genBlock_A(ast* pointer) {
    genBlockItem(pointer->right);
    if (pointer->next != NULL) genBlock_A(pointer->next);
    return 0;
}

// 生成单个代码块项 (声明或语句)
int genBlockItem(ast* pointer) {
    if (strcmp(pointer->node_type, "CONST_DECL") == 0) 
        genLocalConstDecl(pointer);
    else if (strcmp(pointer->node_type, "VAR_DECL") == 0) 
        genLocalVarDecl(pointer);
    else if (strcmp(pointer->node_type, "STMT") == 0) 
        genStmt(pointer);
    return 0;
}

// 生成局部常量声明链表
int genLocalConstDecl(ast* pointer) {
    genLocalConstDef(pointer->right);
    if (pointer->next != NULL) genLocalConstDecl(pointer->next);
    return 0;
}

// 生成局部常量定义
// 输出store指令将常量值存入预分配的槽位
int genLocalConstDef(ast* pointer) {
    if (pointer->left == NULL) {
        // 标量常量
        int slot = get_local_slot(pointer->value_string);
        if (slot == -1) slot = create_local_slot(pointer->value_string);
        flush_pending_allocas();
        
        int val = evalLocalConstExpr(pointer->right->right);
        buf_printf("  store i32 %d, i32* %%%d, align 4\n", val, slot);
        
        // 记录到常量表用于后续常量折叠
        insertKeyValuePair(&const_local, pointer->value_string, val);
    } else {
        // 数组常量
        int* dims = NULL;
        int dim_count = 0;
        collect_const_dims(pointer->left, &dims, &dim_count);
        
        int total = 1;
        for(int i=0; i<dim_count; ++i) total *= dims[i];
        
        int* values = (int*)calloc(total, sizeof(int));
        int index = 0;
        flatten_const_init(pointer->right, values, total, &index);
        
        // 生成数组类型字符串
        char type_buffer[256];
        build_array_type_string(dims, dim_count, type_buffer, sizeof(type_buffer));
        
        // 分配空间
        int slot = scout++; 
        buf_printf("  %%%d = alloca %s, align 16\n", slot, type_buffer);
        
        // 注册局部数组信息
        insertArrayInfo(&array_global, pointer->value_string, dims, dim_count);
        char* name_dup = strdup(pointer->value_string);
        insertKeyValuePair(&id_local, name_dup, slot);

        // 逐个元素赋值 (对于lab这是最稳妥的方法)
        for(int i=0; i<total; ++i) {
            int temp = i;
            int ptr_reg = scout++;
            buf_printf("  %%%d = getelementptr inbounds %s, %s* %%%d, i64 0", 
                       ptr_reg, type_buffer, type_buffer, slot);
            
            // 将线性索引转为多维下标
            int* indices = malloc(sizeof(int) * dim_count);
            for(int d = dim_count - 1; d >= 0; d--) {
                indices[d] = temp % dims[d];
                temp /= dims[d];
            }
            for(int d = 0; d < dim_count; d++) {
                buf_printf(", i64 %d", indices[d]);
            }
            buf_printf("\n");
            free(indices);
            
            buf_printf("  store i32 %d, i32* %%%d, align 4\n", values[i], ptr_reg);
        }

        free(dims);
        free(values);
    }
    return 0;
}

// 生成局部变量声明链表
int genLocalVarDecl(ast* pointer) {
    genLocalVarDef(pointer->right);
    if (pointer->next != NULL) genLocalVarDecl(pointer->next);
    return 0;
}

// 生成局部变量定义
// 输出store指令将初始值存入预分配的槽位
int genLocalVarDef(ast* pointer) {
    if (pointer->left == NULL) {
        int slot = get_local_slot(pointer->value_string);
        if (slot == -1) slot = create_local_slot(pointer->value_string);
        flush_pending_allocas();
        
        if (pointer->right != NULL) {
            if (isRuntimeExpr(pointer->right->right)) {
                // 运行时表达式: 生成代码计算
                int reg = genArithmeticExpr(pointer->right->right);
                buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", reg, slot);
            } else {
                // 编译时常量: 直接存入
                buf_printf("  store i32 %d, i32* %%%d, align 4\n", 
                           evalLocalConstExpr(pointer->right->right), slot);
            }
        }
    }
    // 数组变量逻辑类似genLocalConstDef (此处略)
    return 0;
}

/*
    表达式代码生成
*/

// 生成算术表达式 (加减法层)
// 题目的genArithmeticExpr函数
// 返回值: 结果寄存器号
int genArithmeticExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR") == 0) {
        int lhs_reg = -1, rhs_reg = -1;
        int lhs_val = 0, rhs_val = 0;
        int lhs_is_reg = 0, rhs_is_reg = 0;

        // 处理左操作数
        if (isRuntimeExpr(pointer->left)) {
            lhs_reg = genArithmeticExpr(pointer->left);
            lhs_is_reg = 1;
        } else {
            lhs_val = evalLocalConstExpr(pointer->left);
        }

        // 处理右操作数
        if (isRuntimeMulExpr(pointer->right)) {
            rhs_reg = genMulExpr(pointer->right);
            rhs_is_reg = 1;
        } else {
            rhs_val = evalLocalConstExpr(pointer->right);
        }

        // 生成指令
        int res_reg = scout++;
        if (pointer->value_int == '+') 
            buf_printf("  %%%d = add nsw i32 ", res_reg);
        else 
            buf_printf("  %%%d = sub nsw i32 ", res_reg);

        if (lhs_is_reg) buf_printf("%%%d", lhs_reg); 
        else buf_printf("%d", lhs_val);
        buf_printf(", ");
        if (rhs_is_reg) buf_printf("%%%d", rhs_reg); 
        else buf_printf("%d", rhs_val);
        buf_printf("\n");

        return res_reg;
    } else {
        return genMulExpr(pointer);
    }
}

// 生成乘除模运算表达式
// 返回值: 结果寄存器号
int genMulExpr(ast* pointer) {
    if (strcmp(pointer->node_type, "EXPR_MUL") == 0) {
        int lhs_reg = -1, rhs_reg = -1;
        int lhs_val = 0, rhs_val = 0;
        int lhs_is_reg = 0, rhs_is_reg = 0;

        if (isRuntimeMulExpr(pointer->left)) {
            lhs_reg = genMulExpr(pointer->left);
            lhs_is_reg = 1;
        } else {
            lhs_val = evalLocalConstExpr(pointer->left);
        }

        if (isRuntimeUnaryExpr(pointer->right)) {
            rhs_reg = genUnaryExpr(pointer->right);
            rhs_is_reg = 1;
        } else {
            rhs_val = evalLocalConstExpr(pointer->right);
        }

        int res_reg = scout++;
        switch (pointer->value_int) {
            case '*': buf_printf("  %%%d = mul nsw i32 ", res_reg); break;
            case '/': buf_printf("  %%%d = sdiv i32 ", res_reg); break;
            case '%': buf_printf("  %%%d = srem i32 ", res_reg); break;
        }

        if (lhs_is_reg) buf_printf("%%%d", lhs_reg); 
        else buf_printf("%d", lhs_val);
        buf_printf(", ");
        if (rhs_is_reg) buf_printf("%%%d", rhs_reg); 
        else buf_printf("%d", rhs_val);
        buf_printf("\n");

        return res_reg;
    } else {
        return genUnaryExpr(pointer);
    }
}

// 生成一元运算表达式 (正负号、逻辑非、函数调用、数组访问)
// 返回值: 结果寄存器号
int genUnaryExpr(ast* pointer) {
    if (pointer == NULL) return 0;

    // 处理透明包装节点
    // 比较 或 逻辑表达式可能作为包装层出现在表达式中
    if (strcmp(pointer->node_type, "REL_EXP") == 0 || 
        strcmp(pointer->node_type, "EQ_EXP") == 0 ||
        strcmp(pointer->node_type, "L_AND_EXP") == 0 || 
        strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        
        if (strcmp(pointer->value_string, "") == 0) {
            // 无操作符: 只是语法树的传递层, 递归处理内层
            return genUnaryExpr(pointer->left);
        }
        
        // 有操作符: 需要先计算条件(i1), 然后扩展为整数(i32)
        // 如: (a < b) + 1
        int cond_reg = genCondition(pointer);
        int res_reg = scout++;
        buf_printf("  %%%d = zext i1 %%%d to i32\n", res_reg, cond_reg);
        return res_reg;
    }

    if (strcmp(pointer->node_type, "EXP_WITH_UNARY_OP") == 0) {
        // 一元运算符
        int reg = -1;
        int val = 0;
        int is_reg = 0;
        
        if(isRuntimeUnaryExpr(pointer->right)) {
            reg = genUnaryExpr(pointer->right);
            is_reg = 1;
        } else {
            val = evalLocalConstExpr(pointer->right);
        }
        
        if (pointer->left->value_int == '+') {
            return is_reg ? reg : 0; // 正号无需生成指令
        }
        
        int res_reg = scout++; 
        if (pointer->left->value_int == '-') {
            // 负号: 0 - x
            buf_printf("  %%%d = sub nsw i32 0, ", res_reg);
            if(is_reg) buf_printf("%%%d", reg); 
            else buf_printf("%d", val);
            buf_printf("\n");
        } else if (pointer->left->value_int == '!') {
            // 逻辑非: x == 0 ? 1 : 0
            buf_printf("  %%%d = icmp eq i32 ", res_reg);
            if(is_reg) buf_printf("%%%d", reg); 
            else buf_printf("%d", val);
            buf_printf(", 0\n");
            // i1结果扩展为i32
            int zext_reg = scout++;
            buf_printf("  %%%d = zext i1 %%%d to i32\n", zext_reg, res_reg);
            res_reg = zext_reg; 
        }
        return res_reg;

    } else if (strcmp(pointer->node_type, "FUNC_CALL_ID") == 0) {
        // 函数调用
        char buffer[1024] = {0};
        
        if (pointer->right != NULL) {
            genFuncCallArgs(pointer->right, buffer);
            int res_reg = scout++;
            buf_printf("  %%%d = call i32 @%s(%s)\n", res_reg, pointer->value_string, buffer);
            return res_reg;
        } else {
            int res_reg = scout++;
            buf_printf("  %%%d = call i32 @%s()\n", res_reg, pointer->value_string);
            return res_reg;
        }

    } else if (strcmp(pointer->node_type, "ID_WITH_SUBSCRIPT") == 0) {
        // 数组访问
        struct ArrayInfo* info = getArrayInfo(&array_global, pointer->value_string);
        if (info == NULL) {
            buf_printf("unsupported array identifier %s\n", pointer->value_string);
            exit(1);
        }
        
        char type_buffer[256];
        build_array_type_string(info->dims, info->dim_count, type_buffer, sizeof(type_buffer));
        
        // 检查所有下标是否都是常量
        int all_const = 1;
        ast* check_sub = pointer->right;
        while (check_sub != NULL) {
            if (isRuntimeExpr(check_sub->right)) { 
                all_const = 0; 
                break; 
            }
            check_sub = check_sub->next;
        }

        int is_global = (getValueFromMap(&id_local, pointer->value_string) == -1);

        // 全局数组 + 全常量下标: 使用getelementptr常量表达式
        if (is_global && all_const) {
            int res_reg = scout++;
            buf_printf("  %%%d = load i32, i32* getelementptr inbounds (%s, %s* @%s, i64 0", 
                   res_reg, type_buffer, type_buffer, pointer->value_string);
            ast* sub = pointer->right;
            while (sub != NULL) {
                buf_printf(", i64 %d", evalLocalConstExpr(sub->right));
                sub = sub->next;
            }
            buf_printf("), align 16\n"); 
            return res_reg;
        }

        // 一般情况: 生成getelementptr指令
        int ptr_reg = scout++;
        buf_printf("  %%%d = getelementptr inbounds %s, %s* ", 
                   ptr_reg, type_buffer, type_buffer);
        if (!is_global) buf_printf("%%%d", get_local_slot(pointer->value_string)); 
        else buf_printf("@%s", pointer->value_string);
        buf_printf(", i64 0");
        
        // 逐个处理下标
        ast* sub = pointer->right;
        while (sub != NULL) {
            if (isRuntimeExpr(sub->right)) {
                int idx_reg = genArithmeticExpr(sub->right);
                buf_printf(", i64 %%%d", idx_reg);
            } else {
                buf_printf(", i64 %d", evalLocalConstExpr(sub->right));
            }
            sub = sub->next;
        }
        buf_printf("\n");
        
        // 加载数组元素
        int res_reg = scout++;
        buf_printf("  %%%d = load i32, i32* %%%d, align 4\n", res_reg, ptr_reg);
        return res_reg;

    } else {
        // 其他情况
        if (strcmp(pointer->node_type, "EXPR") == 0) {
            return genArithmeticExpr(pointer);
        } 
        else if (strcmp(pointer->node_type, "IDENTIFIER") == 0) {
            char* temp = pointer->value_string;
            int res_reg = scout++;

            // 优先查找局部变量 (Local First)
            if (getValueFromMap(&id_local, temp) != -1) {
                int slot = get_local_slot(temp);
                buf_printf("  %%%d = load i32, i32* %%%d, align 4\n", res_reg, slot);
            } 
            else if (getValueFromMap(&id_global, temp) != -1) {
                buf_printf("  %%%d = load i32, i32* @%s, align 4\n", res_reg, temp);
            } 
            else {
                fprintf(stderr, "Error: Identifier '%s' not found.\n", temp);
                exit(1);
            }
            return res_reg;
        } 
        else if (strcmp(pointer->node_type, "DECIMAL") == 0) {
            return pointer->value_int;
        }
    }
    return 0;
}

// 生成函数调用参数列表
// 参数 buffer: 输出缓冲区, 格式如"i32 %1, i32 %2"
int genFuncCallArgs(ast* pointer, char* buffer) {
    int len = strlen(buffer);
    int reg = -1;
    int val = 0;
    int is_reg = 0;

    if (isRuntimeExpr(pointer->right)) {
        reg = genArithmeticExpr(pointer->right);
        is_reg = 1;
    } else {
        val = evalLocalConstExpr(pointer->right);
    }

    if (is_reg) sprintf(buffer + len, "i32 %%%d", reg);
    else sprintf(buffer + len, "i32 %d", val);

    if (pointer->next != NULL) {
        sprintf(buffer + strlen(buffer), ", ");
        genFuncCallArgs(pointer->next, buffer);
    }
    return 0;
}

/*
    逻辑表达式代码生成
*/

// 生成逻辑表达式 (关系运算: <, >, <=, >=)
// 这是题目要求的genLogicExpr函数
// 返回值: i1类型的结果寄存器号
int genLogicExpr(ast* pointer) {
    if (!pointer) return 0;
    
    // 透传节点处理
    if (strcmp(pointer->node_type, "REL_EXP") != 0) 
        return genArithmeticExpr(pointer);
    if (strcmp(pointer->value_string, "") == 0) 
        return genLogicExpr(pointer->left);

    // 先计算左操作数, 再计算右操作数 (确保scout顺序正确)
    int lhs_is_reg = isRuntimeExpr(pointer->left);
    int lhs_reg = lhs_is_reg ? genArithmeticExpr(pointer->left) : 0;
    
    int rhs_is_reg = isRuntimeExpr(pointer->right);
    int rhs_reg = rhs_is_reg ? genArithmeticExpr(pointer->right) : 0;

    // 生成比较指令
    int res_reg = scout++;
    buf_printf("  %%%d = ", res_reg);
    
    if (strcmp(pointer->value_string, "<") == 0) 
        buf_printf("icmp slt i32 ");
    else if (strcmp(pointer->value_string, ">") == 0) 
        buf_printf("icmp sgt i32 ");
    else if (strcmp(pointer->value_string, "<=") == 0) 
        buf_printf("icmp sle i32 ");
    else if (strcmp(pointer->value_string, ">=") == 0) 
        buf_printf("icmp sge i32 ");
    
    // 打印操作数
    if (lhs_is_reg) buf_printf("%%%d, ", lhs_reg); 
    else { print_operand(pointer->left); buf_printf(", "); }

    if (rhs_is_reg) buf_printf("%%%d", rhs_reg); 
    else print_operand(pointer->right);
    
    buf_printf("\n");
    return res_reg;
}

// 生成相等性表达式 (==, !=)
// 返回值: i1类型的结果寄存器号
int genEqualityExpr(ast* pointer) {
    if (!pointer) return 0;
    if (strcmp(pointer->node_type, "EQ_EXP") != 0) 
        return genLogicExpr(pointer);
    if (strcmp(pointer->value_string, "") == 0) 
        return genLogicExpr(pointer->left);

    // 递归处理左侧
    int lhs_reg = genEqualityExpr(pointer->left);
    
    // 处理右侧
    int rhs_is_reg = isRuntimeExpr(pointer->right);
    int rhs_reg = 0;
    
    if (rhs_is_reg) {
        // 右侧可能是RelExp或AddExp
        if (strcmp(pointer->right->node_type, "REL_EXP") == 0) 
            rhs_reg = genLogicExpr(pointer->right);
        else 
            rhs_reg = genArithmeticExpr(pointer->right);
    }

    // 生成比较指令
    int res_reg = scout++;
    buf_printf("  %%%d = ", res_reg);
    
    if (strcmp(pointer->value_string, "==") == 0) 
        buf_printf("icmp eq i32 ");
    else if (strcmp(pointer->value_string, "!=") == 0) 
        buf_printf("icmp ne i32 ");

    buf_printf("%%%d, ", lhs_reg);
    
    if (rhs_is_reg) buf_printf("%%%d", rhs_reg);
    else print_operand(pointer->right);
    
    buf_printf("\n");
    return res_reg;
}

// 生成逻辑与表达式 (&&, 实现短路求值)
// A && B: 如果A为假, 直接返回假；否则计算B
// 返回值: i1类型的结果寄存器号
int genLogicalAndExpr(ast* pointer) {
    if (strcmp(pointer->value_string, "") == 0) {
        return genEqualityExpr(pointer->left);
    }
    
    // 短路求值实现: 
    // - 创建临时槽位存储结果
    // - 初始化为false(0)
    // - 如果LHS为真, 跳转计算RHS；否则跳到end
    // - 将RHS结果存入临时槽位
    // - 加载结果并转为i1
    
    int lhs_reg = genCondition(pointer->left);
    
    char* label_lhs_true = generate_label();
    char* label_end = generate_label();
    
    int res_ptr = create_local_slot(NULL); // 创建匿名槽位
    flush_pending_allocas();
    
    // 初始化为false
    buf_printf("  store i32 0, i32* %%%d, align 4\n", res_ptr); 
    
    // 条件跳转
    buf_printf("  br i1 %%%d, label %%%s, label %%%s\n", lhs_reg, label_lhs_true, label_end);
    
    // LHS为真时计算RHS
    buf_printf("%s:\n", label_lhs_true);
    int rhs_reg = genCondition(pointer->right);
    int rhs_val = scout++;
    buf_printf("  %%%d = zext i1 %%%d to i32\n", rhs_val, rhs_reg);
    buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", rhs_val, res_ptr);
    buf_printf("  br label %%%s\n", label_end);
    
    // 汇合点: 加载结果
    buf_printf("%s:\n", label_end);
    int final_val = scout++;
    buf_printf("  %%%d = load i32, i32* %%%d, align 4\n", final_val, res_ptr);
    int final_res = scout++;
    buf_printf("  %%%d = icmp ne i32 %%%d, 0\n", final_res, final_val);
    
    return final_res;
}

// 生成逻辑或表达式 (||, 实现短路求值)
// A || B: 如果A为真, 直接返回真；否则计算B
// 返回值: i1类型的结果寄存器号
int genLogicalOrExpr(ast* pointer) {
    if (strcmp(pointer->value_string, "") == 0) {
        return genLogicalAndExpr(pointer->left);
    }
    
    int lhs_reg = genCondition(pointer->left);
    
    char* label_lhs_false = generate_label();
    char* label_end = generate_label();
    
    int res_ptr = create_local_slot(NULL);
    flush_pending_allocas();
    
    // 初始化为true
    buf_printf("  store i32 1, i32* %%%d, align 4\n", res_ptr);
    
    // 如果LHS为真, 直接跳到结束；否则计算RHS
    buf_printf("  br i1 %%%d, label %%%s, label %%%s\n", lhs_reg, label_end, label_lhs_false);
    
    buf_printf("%s:\n", label_lhs_false);
    int rhs_reg = genCondition(pointer->right);
    int rhs_val = scout++;
    buf_printf("  %%%d = zext i1 %%%d to i32\n", rhs_val, rhs_reg);
    buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", rhs_val, res_ptr);
    buf_printf("  br label %%%s\n", label_end);
    
    buf_printf("%s:\n", label_end);
    int final_val = scout++;
    buf_printf("  %%%d = load i32, i32* %%%d, align 4\n", final_val, res_ptr);
    int final_res = scout++;
    buf_printf("  %%%d = icmp ne i32 %%%d, 0\n", final_res, final_val);
    
    return final_res;
}

// 生成条件表达式 (统一入口, 确保最终得到i1类型)
// 处理各种类型的条件: 逻辑或/与、比较、数值
// 返回值: i1类型的结果寄存器号
int genCondition(ast* pointer) {
    if (!pointer) return 0;
    
    if (strcmp(pointer->node_type, "L_OR_EXP") == 0) 
        return genLogicalOrExpr(pointer);
    if (strcmp(pointer->node_type, "L_AND_EXP") == 0) 
        return genLogicalAndExpr(pointer);
    
    if (strcmp(pointer->node_type, "EQ_EXP") == 0) {
        int reg = genEqualityExpr(pointer);
        
        // 检查是否是"伪"EqExp (只是传递数值的节点)
        int is_val = 0;
        if (strcmp(pointer->value_string, "") == 0) {
            ast* rel = pointer->left;
            if (rel && strcmp(rel->node_type, "REL_EXP") == 0 && 
                strcmp(rel->value_string, "") == 0) {
                is_val = 1;
            }
        }
        
        if (is_val) {
            // 传递的是i32数值, 需要转i1
            int res = scout++;
            buf_printf("  %%%d = icmp ne i32 %%%d, 0\n", res, reg);
            return res;
        }
        return reg;
    }
    
    if (strcmp(pointer->node_type, "REL_EXP") == 0) {
        int reg = genLogicExpr(pointer);
        if (strcmp(pointer->value_string, "") == 0) {
            // 传递的是i32数值, 需要转i1
            int res = scout++;
            buf_printf("  %%%d = icmp ne i32 %%%d, 0\n", res, reg);
            return res;
        }
        return reg;
    }
    
    // 普通数值表达式
    int val_reg;
    if (isRuntimeExpr(pointer)) {
        val_reg = genArithmeticExpr(pointer);
    } else {
        val_reg = scout++;
        buf_printf("  %%%d = add i32 0, %d\n", val_reg, evalLocalConstExpr(pointer));
    }
    int res = scout++;
    buf_printf("  %%%d = icmp ne i32 %%%d, 0\n", res, val_reg);
    return res;
}

/*
    带跳转的条件生成 (用于 if 和 while)
*/

// 占位符计数器, 确保每个if/while的占位符唯一
int ph_counter = 0;
// 循环元数据计数器
static int loop_metadata_counter = 6;

// 生成带占位符的条件跳转 (字符串版本)
// 用于if/while语句中, 先生成占位符, 后续回填真实标签号
// 参数 true_label_ph True跳转目标的占位符 (如"{{T_1}}")
// 参数 false_label_ph False跳转目标的占位符 (如"{{F_1}}")
void genCondWithJumpPlaceholder(ast* pointer, const char* true_label_ph, const char* false_label_ph) {
    if (!pointer) return;

    // 处理逻辑或 (A || B)
    if (strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            genCondWithJumpPlaceholder(pointer->left, true_label_ph, false_label_ph);
            return;
        }
        
        int left_is_simple = (strcmp(pointer->left->node_type, "L_OR_EXP") != 0 && 
                              strcmp(pointer->left->node_type, "L_AND_EXP") != 0);
        
        if (left_is_simple) {
            // 简单左子树: 直接生成条件和跳转
            int cond = genCondition(pointer->left);
            int check_b = scout++;
            buf_printf("  br i1 %%%d, label %%%s, label %%%d\n", cond, true_label_ph, check_b);
            buf_printf("\n%d:\n", check_b);
        } else {
            // 复杂左子树: 递归处理
            char check_b_ph[20];
            sprintf(check_b_ph, "{{CB_%d}}", scout);
            genCondWithJumpPlaceholder(pointer->left, true_label_ph, check_b_ph);
            
            int check_b = scout++;
            backpatch_label(check_b_ph, check_b);
            buf_printf("\n%d:\n", check_b);
        }
        
        // 生成右子树
        genCondWithJumpPlaceholder(pointer->right, true_label_ph, false_label_ph);
        return;
    }

    // 处理逻辑与 (A && B)
    if (strcmp(pointer->node_type, "L_AND_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            genCondWithJumpPlaceholder(pointer->left, true_label_ph, false_label_ph);
            return;
        }
        
        int left_is_simple = (strcmp(pointer->left->node_type, "L_OR_EXP") != 0 && 
                              strcmp(pointer->left->node_type, "L_AND_EXP") != 0);
        
        if (left_is_simple) {
            int cond = genCondition(pointer->left);
            int check_b = scout++;
            buf_printf("  br i1 %%%d, label %%%d, label %%%s\n", cond, check_b, false_label_ph);
            buf_printf("\n%d:\n", check_b);
        } else {
            char check_b_ph[20];
            sprintf(check_b_ph, "{{CB_%d}}", scout);
            genCondWithJumpPlaceholder(pointer->left, check_b_ph, false_label_ph);
            
            int check_b = scout++;
            backpatch_label(check_b_ph, check_b);
            buf_printf("\n%d:\n", check_b);
        }
        
        genCondWithJumpPlaceholder(pointer->right, true_label_ph, false_label_ph);
        return;
    }

    // 基础条件: 直接生成跳转
    int cond_reg = genCondition(pointer);
    buf_printf("  br i1 %%%d, label %%%s, label %%%s\n", cond_reg, true_label_ph, false_label_ph);
}

// 生成带整数标签的条件跳转 (用于特殊情况)
// 与字符串版本的区别: 支持负数标签作为特殊占位符
void genCondWithJump(ast* pointer, int true_label, int false_label) {
    if (!pointer) return;

    // 打印标签引用的宏 (处理特殊占位符)
    #define PRINT_LABEL_REF(lbl, type) \
        do { \
            if (lbl == -100) buf_printf("%%{{T}}"); \
            else if (lbl == -101) buf_printf("%%{{F}}"); \
            else if (lbl == -102) buf_printf("%%{{E}}"); \
            else buf_printf("%%%d", lbl); \
        } while(0)

    // 逻辑或
    if (strcmp(pointer->node_type, "L_OR_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            genCondWithJump(pointer->left, true_label, false_label);
            return;
        }
        
        int left_is_simple = (strcmp(pointer->left->node_type, "L_OR_EXP") != 0 && 
                              strcmp(pointer->left->node_type, "L_AND_EXP") != 0);
        
        if (left_is_simple) {
            int left_cond = genCondition(pointer->left);
            int check_b = scout++;
            buf_printf("  br i1 %%%d, label ", left_cond);
            PRINT_LABEL_REF(true_label, 0);
            buf_printf(", label %%%d\n", check_b);
            
            buf_printf("\n%d:\n", check_b);
            genCondWithJump(pointer->right, true_label, false_label);
        } else {
            int check_b = scout++;
            genCondWithJump(pointer->left, true_label, check_b);
            buf_printf("\n%d:\n", check_b);
            genCondWithJump(pointer->right, true_label, false_label);
        }
        return;
    }

    // 逻辑与
    if (strcmp(pointer->node_type, "L_AND_EXP") == 0) {
        if (strcmp(pointer->value_string, "") == 0) {
            genCondWithJump(pointer->left, true_label, false_label);
            return;
        }
        
        int left_is_simple = (strcmp(pointer->left->node_type, "L_OR_EXP") != 0 && 
                              strcmp(pointer->left->node_type, "L_AND_EXP") != 0);
        
        if (left_is_simple) {
            int left_cond = genCondition(pointer->left);
            int check_b = scout++;
            buf_printf("  br i1 %%%d, label %%%d, label ", left_cond, check_b);
            PRINT_LABEL_REF(false_label, 1);
            buf_printf("\n");
            
            buf_printf("\n%d:\n", check_b);
            genCondWithJump(pointer->right, true_label, false_label);
        } else {
            int check_b = scout++;
            genCondWithJump(pointer->left, check_b, false_label);
            buf_printf("\n%d:\n", check_b);
            genCondWithJump(pointer->right, true_label, false_label);
        }
        return;
    }

    // 基础比较
    int cond_reg = genCondition(pointer);
    buf_printf("  br i1 %%%d, label ", cond_reg);
    PRINT_LABEL_REF(true_label, 0);
    buf_printf(", label ");
    PRINT_LABEL_REF(false_label, 1);
    buf_printf("\n");
    
    #undef PRINT_LABEL_REF
}

/*
    语句代码生成
*/

// 前向声明
int genStmt(ast* pointer);
void genIfElseChain(ast* pointer, const char* end_label);

// if-else链处理 (没用到)
void genIfElseChain(ast* pointer, const char* end_label) {
    if (pointer == NULL) return;
    if (strcmp(pointer->value_string, "if") == 0) {
        int then_label = scout++;
        int else_label = scout++;

        int cond_reg = genCondition(pointer->left);
        buf_printf("  br i1 %%%d, label %%%d, label %%%d\n", cond_reg, then_label, else_label);

        // then分支
        buf_printf("\n%d:\n", then_label);
        genStmt(pointer->right);
        buf_printf("  br label %%%s\n", end_label);

        // else分支
        buf_printf("\n%d:\n", else_label);
        if (pointer->extend && strcmp(pointer->extend->value_string, "if") == 0) {
            // else if情况: 递归处理下一个if条件
            genIfElseChain(pointer->extend, end_label);
        } else if (pointer->extend) {
            // 纯else情况
            genStmt(pointer->extend);
            buf_printf("  br label %%%s\n", end_label);
        }
    }
}

// 生成语句 (总入口)
// 分发到具体的语句生成函数
// 返回值: 0表示成功
int genStmt(ast* pointer) {
    if (pointer == NULL) return 0;
    if (strcmp(pointer->node_type, "BLOCK") == 0) { 
        genBlock(pointer); 
        return 0; 
    }
    if (pointer->value_string == NULL) return 0;
    
    // 空语句或表达式语句
    if (strcmp(pointer->value_string, "") == 0) {
        if (pointer->left) {
            if (strcmp(pointer->left->node_type, "BLOCK") == 0) 
                genBlock(pointer->left);
            else if (isRuntimeExpr(pointer->left)) 
                genArithmeticExpr(pointer->left);
        }
        return 0;
    }

    if (strcmp(pointer->value_string, "if") == 0) {
        /*
            IF语句生成    
            题目的genIfStmt的实现
        */

        // 检查条件是否包含短路运算
        int has_complex = (pointer->left && 
                          (strcmp(pointer->left->node_type, "L_OR_EXP") == 0 || 
                           strcmp(pointer->left->node_type, "L_AND_EXP") == 0));
        int has_else = (pointer->extend != NULL);

        // 生成唯一的占位符 (避免多个if语句冲突)
        int current_id = ph_counter++;
        char t_ph[32], f_ph[32], e_ph[32];
        sprintf(t_ph, "{{T_%d_______}}", current_id);
        sprintf(f_ph, "{{F_%d_______}}", current_id);
        sprintf(e_ph, "{{E_%d_______}}", current_id);

        const char* target_false_ph = has_else ? f_ph : e_ph;
        
        // 生成条件跳转
        if (!has_complex) {
            // 简单条件: 直接计算并跳转
            int cond_reg = genCondition(pointer->left);
            buf_printf("  br i1 %%%d, label %%%s, label %%%s\n", 
                       cond_reg, t_ph, target_false_ph);
        } else {
            // 复杂条件 (包含&&或||): 使用短路求值
            genCondWithJumpPlaceholder(pointer->left, t_ph, target_false_ph);
        }

        // 生成Then分支
        int real_true_label = scout++;
        buf_printf("\n%d:\n", real_true_label);
        genStmt(pointer->right);
        
        // 如果then分支不以return结尾, 需要跳到end
        if (!ends_with_return(pointer->right)) {
            buf_printf("  br label %%%s\n", e_ph);
        }

        // 生成Else分支 (如果有)
        int real_false_label = -1;
        if (has_else) {
            real_false_label = scout++;
            buf_printf("\n%d:\n", real_false_label);
            
            if (strcmp(pointer->extend->node_type, "BLOCK") == 0) 
                genBlock(pointer->extend);
            else 
                genStmt(pointer->extend);
            
            if (!ends_with_return(pointer->extend)) {
                buf_printf("  br label %%%s\n", e_ph);
            }
        }

        // 生成End标签 (汇合点)
        int real_end_label = scout++;
        buf_printf("\n%d:\n", real_end_label);

        // 回填占位符
        backpatch_label(t_ph, real_true_label);
        if (has_else) backpatch_label(f_ph, real_false_label);
        backpatch_label(e_ph, real_end_label);
        
        return 0;
    } else if (strcmp(pointer->value_string, "while") == 0) {
        /*
            WHILE语句生成
            题目的genWhileStmt的实现
        */

        // 生成Start标签 (循环开始)
        int real_start_label = scout++;
        buf_printf("  br label %%%d\n", real_start_label);
        buf_printf("\n%d:\n", real_start_label);
        
        // 准备占位符
        int current_id = ph_counter++;
        char wb_ph[32], we_ph[32];
        sprintf(wb_ph, "{{WB_%d_______}}", current_id);
        sprintf(we_ph, "{{WE_%d_______}}", current_id);
    
        // 生成条件跳转
        int has_complex = (pointer->left && 
                          (strcmp(pointer->left->node_type, "L_OR_EXP") == 0 || 
                           strcmp(pointer->left->node_type, "L_AND_EXP") == 0));
        
        if (!has_complex) {
            int cond_reg = genCondition(pointer->left);
            buf_printf("  br i1 %%%d, label %%%s, label %%%s\n", 
                       cond_reg, wb_ph, we_ph);
        } else {
            genCondWithJumpPlaceholder(pointer->left, wb_ph, we_ph);
        }
        
        // 生成Body块
        int real_body_label = scout++;
        buf_printf("\n%d:\n", real_body_label);
        
        // 先递归生成循环体 (确保内层循环先获得ID)
        genStmt(pointer->right);

        // 递归回来后再分配当前循环的metadata ID
        int current_loop_id = loop_metadata_counter;
        loop_metadata_counter += 2; 
        
        // 循环体结束跳回start (带循环元数据 !llvm.loop)
        if (!ends_with_return(pointer->right)) {
            buf_printf("  br label %%%d, !llvm.loop !%d\n", 
                       real_start_label, current_loop_id);
        }
        
        // 生成End标签 (循环退出)
        int real_end_label = scout++;
        buf_printf("\n%d:\n", real_end_label);
        
        // 回填占位符
        backpatch_label(wb_ph, real_body_label);
        backpatch_label(we_ph, real_end_label);
        
        return 0;
    } else if (strcmp(pointer->value_string, "return") == 0) {
        /*
            RETURN语句
        */

        int ret_slot = get_local_slot("__ret");
        
        if (func_exit_label == -99) {
            // 复杂函数模式: 跳转到EXIT占位符
            if (pointer->left == NULL) {
                buf_printf("  br label %%{{EXIT}} \n");
            } else if (isRuntimeExpr(pointer->left)) {
                int reg = genArithmeticExpr(pointer->left);
                buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", reg, ret_slot);
                buf_printf("  br label %%{{EXIT}} \n");
            } else {
                buf_printf("  store i32 %d, i32* %%%d, align 4\n", 
                           evalLocalConstExpr(pointer->left), ret_slot);
                buf_printf("  br label %%{{EXIT}} \n");
            }
        } else {
            // 简单函数模式: 直接返回
            if (pointer->left == NULL) 
                buf_printf("  ret void\n");
            else if (isRuntimeExpr(pointer->left)) 
                buf_printf("  ret i32 %%%d\n", genArithmeticExpr(pointer->left));
            else 
                buf_printf("  ret i32 %d\n", evalLocalConstExpr(pointer->left));
        }
        return 0;
    } else if (strcmp(pointer->value_string, "=") == 0) {
        /*
            赋值语句
            题目的genAssignStmt的实现
        */

        ast* lval = pointer->left;
        int is_local = 1; 
        int slot = -1; 
        char* name = NULL;
        
        // 处理左值
        if (strcmp(lval->node_type, "IDENTIFIER") == 0) {
            // 标量变量
            name = lval->value_string;
            slot = get_local_slot(name);
            if (slot == -1) { 
                // 检查是否是全局变量
                if (getValueFromMap(&id_global, name) != -1) 
                    is_local = 0; 
                else 
                    slot = create_local_slot(name); 
            }
        } else if (strcmp(lval->node_type, "ID_WITH_SUBSCRIPT") == 0) {
            // 数组元素
            struct ArrayInfo* info = getArrayInfo(&array_global, lval->value_string);
            char type_buffer[256]; 
            build_array_type_string(info->dims, info->dim_count, 
                                   type_buffer, sizeof(type_buffer));
            
            // 生成getelementptr
            int ptr_reg = scout++;
            int is_g = (get_local_slot(lval->value_string) == -1);
            buf_printf("  %%%d = getelementptr inbounds %s, %s* ", 
                       ptr_reg, type_buffer, type_buffer);
            
            if (!is_g) 
                buf_printf("%%%d", get_local_slot(lval->value_string)); 
            else 
                buf_printf("@%s", lval->value_string);
            buf_printf(", i64 0");
            
            // 处理下标
            ast* sub = lval->right;
            while(sub != NULL) {
                if(isRuntimeExpr(sub->right)) 
                    buf_printf(", i64 %%%d", genArithmeticExpr(sub->right)); 
                else 
                    buf_printf(", i64 %d", evalLocalConstExpr(sub->right));
                sub = sub->next;
            }
            buf_printf("\n");
            
            // 存储右值
            if (isRuntimeExpr(pointer->right)) 
                buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", 
                           genArithmeticExpr(pointer->right), ptr_reg);
            else 
                buf_printf("  store i32 %d, i32* %%%d, align 4\n", 
                           evalLocalConstExpr(pointer->right), ptr_reg);
            return 0;
        }
        
        // 处理右值并存储
        if (isRuntimeExpr(pointer->right)) {
            int reg = genArithmeticExpr(pointer->right);
            if (is_local) 
                buf_printf("  store i32 %%%d, i32* %%%d, align 4\n", reg, slot); 
            else 
                buf_printf("  store i32 %%%d, i32* @%s, align 4\n", reg, name);
        } else {
            int value = evalLocalConstExpr(pointer->right);
            if (is_local) 
                buf_printf("  store i32 %d, i32* %%%d, align 4\n", value, slot); 
            else 
                buf_printf("  store i32 %d, i32* @%s, align 4\n", value, name);
        }
        return 0;
    } else if (strcmp(pointer->value_string, "{") == 0) {
        /*
            代码块语句
        */

        genBlock(pointer);
        return 0;
    } else if (strcmp(pointer->value_string, "putint") == 0) {
        /*
            PUTINT语句
        */

        if (pointer->left == NULL) {
            fprintf(stderr, "Error: putint without argument\n");
            exit(1);
        }
        
        ast* arg = pointer->left;
        
        // 判断参数是否是函数调用
        int is_func_call = (strcmp(arg->node_type, "FUNC_CALL_ID") == 0);
        
        // 先计算参数 (确保scout顺序正确)
        int arg_reg = -1;
        int arg_val = 0;
        int is_const = 0;
        
        if (isRuntimeExpr(arg)) {
            arg_reg = genArithmeticExpr(arg);
        } else {
            arg_val = evalLocalConstExpr(arg);
            is_const = 1;
        }
        
        // 再分配返回值寄存器
        int res_reg = scout++;
        
        // 生成调用指令
        if (is_func_call) {
            // 函数调用: call i32 @putint
            if (is_const) {
                buf_printf("  %%%d = call i32 @putint(i32 %d)\n", res_reg, arg_val);
            } else {
                buf_printf("  %%%d = call i32 @putint(i32 %%%d)\n", res_reg, arg_reg);
            }
        } else {
            // 变量 或 常量: call @putint
            if (is_const) {
                buf_printf("  %%%d = call @putint(i32 %d)\n", res_reg, arg_val);
            } else {
                buf_printf("  %%%d = call @putint(i32 %%%d)\n", res_reg, arg_reg);
            }
        }
        return 0;
    } else if (strcmp(pointer->value_string, "") != 0 && 
             strcmp(pointer->value_string, ";") != 0) {
        /*
            其他语句
        */

        // 表达式语句 (仅计算)
        if (pointer->left != NULL && isRuntimeExpr(pointer->left)) 
            genArithmeticExpr(pointer->left);
    }
    
    return 0;
}

/*
    主代码
    LLVM IR代码生成主函数
    参数 root: AST根节点
    返回值: 0表示成功
*/

int genllvm(ast* root) {
    // 初始化缓冲区
    init_buffer();
    
    // 初始化符号表
    ast* cruiser = root;
    initializeMap(&id_global);
    initializeMap(&const_global);
    initializeArrayMap(&array_global);
    
    // 遍历编译单元, 生成代码
    while (cruiser != NULL) {
        if (strcmp(cruiser->node_type, "COMP_UNIT") == 0) {
            if (strcmp(cruiser->right->node_type, "CONST_DECL") == 0) {
                genGlobalConstDecl(cruiser->right);
            } else if (strcmp(cruiser->right->node_type, "VAR_DECL") == 0) {
                genGlobalVarDecl(cruiser->right);
            } else if (strcmp(cruiser->right->node_type, "FUNC_DEF") == 0) {
                genFuncDef(cruiser->right);
            }
        } else {
            fprintf(stderr, "DO NOT DETECT COMP_UNIT\n");
            exit(0);
        }
        cruiser = cruiser->next;
    }
    
    // 输出所有缓冲内容
    flush_buffer();
    
    // 清理资源
    cleanup_buffer();
    cleanupArrayMap(&array_global);
    cleanupMap(&const_global);
    
    return 0;
}