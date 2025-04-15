/**
 * @file shell.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief letter shell
 * @version 2.0.0
 * @date 2018-12-29
 *
 * @Copyright (c) 2018 Letter
 *
 */

#include "wmsdk_config.h"

#include "string.h"
#include "stdio.h"
#include "stdarg.h"
#include "stdint.h"
#include "letter_shell.h"

#if SHELL_AUTO_PRASE == 1
#include "letter_shell_ext.h"
#endif

/**
 * @brief shell��ʾ��Ϣ�ı�����
 */
enum
{
    TEXT_INFO,
    TEXT_PWD_HINT,
    TEXT_PWD_RIGHT,
    TEXT_PWD_ERROR,
    TEXT_FUN_LIST,
    TEXT_VAR_LIST,
    TEXT_CMD_NONE,
    TEXT_CMD_TOO_LONG,
    TEXT_READ_NOT_DEF,
};

/**
 * @brief shell��ʾ��Ϣ�ı�
 */
static const char *shellText[] =
{
#if CONFIG_CLI_BOOT_BANNER
    [TEXT_INFO]      = "\r\n\r\n"
                       " __          ____  __   _____   _______      ____   _____    _____ _          _ _ \r\n"
                       " \\ \\        / /  \\/  | |_   _| |__   __|    / __ \\ / ____|  / ____| |        | | |\r\n"
                       "  \\ \\  /\\  / /| \\  / |   | |  ___ | |______| |  | | (___   | (___ | |__   ___| | |\r\n"
                       "   \\ \\/  \\/ / | |\\/| |   | | / _ \\| |______| |  | |\\___ \\   \\___ \\| '_ \\ / _ \\ | |\r\n"
                       "    \\  /\\  /  | |  | |  _| || (_) | |      | |__| |____) |  ____) | | | |  __/ | |\r\n"
                       "     \\/  \\/   |_|  |_| |_____\\___/|_|       \\____/|_____/  |_____/|_| |_|\\___|_|_|\r\n"
                       "                                                                                  \r\n",
#else
    [TEXT_INFO]      = "",
#endif
    [TEXT_PWD_HINT]  = "\r\nPlease input password:",
    [TEXT_PWD_RIGHT] = "\r\npassword confirm success.\r\n",
    [TEXT_PWD_ERROR] = "\r\npassword confirm failed.\r\n",
    [TEXT_FUN_LIST]  = "\r\nCOMMAND LIST:\r\n\r\n",
    [TEXT_VAR_LIST]  = "\r\nVARIABLE LIST:\r\n\r\n",
    [TEXT_CMD_NONE]  = "Command not found\r\n",
    [TEXT_CMD_TOO_LONG] = "\r\nWarnig: Command is too long\r\n",
    [TEXT_READ_NOT_DEF] = "error: shell.read must be defined\r\n",
};


static SHELL_TypeDef *shellList[SHELL_MAX_NUMBER] = {NULL};     /**< shell�б� */

static void shellAdd(SHELL_TypeDef *shell);
static void shellDisplayItem(SHELL_TypeDef *shell, unsigned short index);

static void shellEnter(SHELL_TypeDef *shell);
static void shellTab(SHELL_TypeDef *shell);
static void shellBackspace(SHELL_TypeDef *shell);
static void shellAnsiStart(SHELL_TypeDef *shell);

#if SHELL_USING_VAR == 1
static void shellDisplayVariable(SHELL_TypeDef *shell, char *var);
void shellListVariables(void);
#endif /** SHELL_USING_VAR == 1 */

#if SHELL_USING_CMD_EXPORT != 1
/**
 * @brief shellĬ�������
 *
 * @note ��ʹ�������ʽ���������ʱ�򣬴˱�Ż���Ч
 * @note �������ʱ����ʹ��SHELL_CMD_ITEM�꣬��SHELL_CMD_ITEM(help, shellHelp, command help)
 * @note �ɲ�ʹ��Ĭ���������ʼ�����֮�󣬿ɵ���shellSetCommandList�ӿ����������
 */
const SHELL_CommandTypeDef shellDefaultCommandList[] =
{
    SHELL_CMD_ITEM_EX(help, shellHelp, command help, help [command] --show help info of command),
#if SHELL_USING_VAR == 1
    SHELL_CMD_ITEM(vars, shellListVariables, show vars),
    SHELL_CMD_ITEM_EX(setVar, shellSetVariable, set var, setVar $[var] [value]),
#endif /** SHELL_USING_VAR == 1 */
    SHELL_CMD_ITEM(cls, shellClear, clear command line),
};

#if SHELL_USING_VAR == 1
/**
 * @brief shellĬ�ϱ�����
 *
 */
const SHELL_VaribaleTypeDef shellDefaultVariableList[] =
{

};
#endif /** SHELL_USING_VAR == 1 */
#endif /** SHELL_USING_CMD_EXPORT != 1 */

/**
 * @brief Ĭ�ϰ�����Ӧӳ�亯����
 *
 */
const SHELL_KeyFunctionDef shellDefaultKeyFunctionList[] =
{
    {SHELL_KEY_LF,          shellEnter},
    {SHELL_KEY_CR,          shellEnter},
    {SHELL_KEY_TAB,         shellTab},
    {SHELL_KEY_BACKSPACE,   shellBackspace},
    {SHELL_KEY_DELETE,      shellBackspace},
    {SHELL_KEY_ESC,         shellAnsiStart},
};


/**
 * @brief shell��ʼ��
 *
 * @param shell shell����
 */
void shellInit(SHELL_TypeDef *shell)
{
    shell->length = 0;
    shell->cursor = 0;
    shell->historyCount = 0;
    shell->historyFlag = 0;
    shell->historyOffset = 0;
    shell->status.inputMode = SHELL_IN_NORMAL;
    shell->status.isActive = 0;
    shell->status.tabFlag = 0;
    shell->command = SHELL_DEFAULT_COMMAND;
    shellAdd(shell);

#if SHELL_USING_AUTH == 1
    shell->status.authFlag = 0;
    shellDisplay(shell, shellText[TEXT_PWD_HINT]);
#else
    shellDisplay(shell, shellText[TEXT_INFO]);
    shellDisplay(shell, shell->command);
#endif

#if SHELL_USING_CMD_EXPORT == 1
    #if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && __ARMCC_VERSION >= 6000000)
        extern const unsigned int shellCommand$$Base;
        extern const unsigned int shellCommand$$Limit;
        extern const unsigned int shellVariable$$Base;
        extern const unsigned int shellVariable$$Limit;

        shell->commandBase = (SHELL_CommandTypeDef *)(&shellCommand$$Base);
        shell->commandNumber = ((unsigned int)(&shellCommand$$Limit)
                                - (unsigned int)(&shellCommand$$Base))
                                / sizeof(SHELL_CommandTypeDef);
        #if SHELL_USING_VAR == 1
            shell->variableBase = (SHELL_VaribaleTypeDef *)(&shellVariable$$Base);
            shell->variableNumber = ((unsigned int)(&shellVariable$$Limit)
                                    - (unsigned int)(&shellVariable$$Base))
                                    / sizeof(SHELL_VaribaleTypeDef);
        #endif /** SHELL_USING_VAR == 1 */

    #elif defined(__ICCARM__)
        shell->commandBase = (SHELL_CommandTypeDef *)(__section_begin(WM_CLI_CMD_SECTION));
        shell->commandNumber = ((unsigned int)(__section_end(WM_CLI_CMD_SECTION))
                                - (unsigned int)(__section_begin(WM_CLI_CMD_SECTION)))
                                / sizeof(SHELL_CommandTypeDef);
        #if SHELL_USING_VAR == 1
            shell->variableBase = (SHELL_VaribaleTypeDef *)(__section_begin("shellVariable"));
            shell->variableNumber = ((unsigned int)(__section_end("shellVariable"))
                                    - (unsigned int)(__section_begin("shellVariable")))
                                    / sizeof(SHELL_VaribaleTypeDef);
        #endif /** SHELL_USING_VAR == 1 */
    #elif defined(__GNUC__)
        extern const unsigned int wm_cli_cmd_start;
        extern const unsigned int wm_cli_cmd_end;

        shell->commandBase = (SHELL_CommandTypeDef *)(&wm_cli_cmd_start);
        shell->commandNumber = ((unsigned int)(&wm_cli_cmd_end)
                                - (unsigned int)(&wm_cli_cmd_start))
                                / sizeof(SHELL_CommandTypeDef);
        #if SHELL_USING_VAR == 1
            extern const unsigned int _shell_variable_start;
            extern const unsigned int _shell_variable_end;
            shell->variableBase = (SHELL_VaribaleTypeDef *)(&_shell_variable_start);
            shell->variableNumber = ((unsigned int)(&_shell_variable_end)
                                    - (unsigned int)(&_shell_variable_start))
                                    / sizeof(SHELL_VaribaleTypeDef);
        #endif /** SHELL_USING_VAR == 1 */
    #else
        #error not supported compiler, please use command table mode
    #endif
#else
    shell->commandBase = (SHELL_CommandTypeDef *)shellDefaultCommandList;
    shell->commandNumber = sizeof(shellDefaultCommandList) / sizeof(SHELL_CommandTypeDef);
    #if SHELL_USING_VAR == 1
        shell->variableBase = (SHELL_VaribaleTypeDef *)shellDefaultVariableList;
        shell->variableNumber = sizeof(shellDefaultVariableList) / sizeof(SHELL_VaribaleTypeDef);
    #endif /** SHELL_USING_VAR == 1 */
#endif
}


#if SHELL_USING_CMD_EXPORT != 1
/**
 * @brief shell���������
 *
 * @param shell shell����
 * @param base ������ַ
 * @param size ��������
 *
 * @note �˽ӿڲ�����shellInit֮ǰ����
 * @note �����ô˽ӿڣ���ʹ��Ĭ��������������γɵ������(ȡ��������巽ʽ)
 */
void shellSetCommandList(SHELL_TypeDef *shell, SHELL_CommandTypeDef *base, unsigned short size)
{
    shell->commandBase = base;
    shell->commandNumber = size;
}


#if SHELL_USING_VAR == 1
/**
 * @brief shell���ñ�����
 *
 * @param shell shell����
 * @param base �������ַ
 * @param size ��������
 *
 * @note �˽ӿڲ�����shellInit֮ǰ����
 * @note �����ô˽ӿڣ���ʹ��Ĭ��������������γɵ������(ȡ��������巽ʽ)
 */
void shellSetVariableList(SHELL_TypeDef *shell, SHELL_VaribaleTypeDef *base, unsigned short size)
{
    shell->variableBase = base;
    shell->variableNumber = size;
}
#endif /** SHELL_USING_VAR == 1 */
#endif /** SHELL_USING_CMD_EXPORT != 1 */


/**
 * @brief shell���ð�����Ӧ
 *
 * @param shell shell����
 * @param base ������Ӧ���ַ
 * @param size ������Ӧ����
 */
void shellSetKeyFuncList(SHELL_TypeDef *shell, SHELL_KeyFunctionDef *base, unsigned short size)
{
    shell->keyFuncBase = (int)base;
    shell->keyFuncNumber = size;
}


/**
 * @brief ���shell��shell�б�
 *
 * @param shell shell����
 */
static void shellAdd(SHELL_TypeDef *shell)
{
    for (short i = 0; i < SHELL_MAX_NUMBER; i++)
    {
        if (shellList[i] == NULL)
        {
            shellList[i] = shell;
            return;
        }
    }
}


/**
 * @brief ��ȡ��ǰ�shell
 *
 * @return SHELL_TypeDef* ��ǰ�shell����
 */
SHELL_TypeDef *shellGetCurrent(void)
{
    for (short i = 0; i < SHELL_MAX_NUMBER; i++)
    {
        if (shellList[i] != NULL && shellList[i]->status.isActive == 1)
        {
            return shellList[i];
        }
    }
    return NULL;
}


#if SHELL_PRINT_BUFFER > 0
/**
 * @brief shell��ʽ�����
 *
 * @param shell shell����
 * @param fmt ��ʽ���ַ���
 * @param ... ����
 */
void shellPrint(SHELL_TypeDef *shell, char *fmt, ...)
{
    char buffer[SHELL_PRINT_BUFFER];
    va_list vargs;

    if (!shell)
    {
        return;
    }

    va_start(vargs, fmt);
    vsnprintf(buffer, SHELL_PRINT_BUFFER - 1, fmt, vargs);
    va_end(vargs);

    shellDisplay(shell, buffer);
}
#endif


/**
 * @brief shell��ʾ�ַ���
 *
 * @param shell shell����
 * @param string �ַ���
 * @return unsigned short �ַ�������
 */
unsigned short shellDisplay(SHELL_TypeDef *shell, const char *string)
{
    unsigned short count = 0;
    if (shell->write == NULL)
    {
        return 0;
    }
    while(*string)
    {
        shell->write(*string++);
        count ++;
    }
    return count;
}


/**
 * @brief shell��ʾ�ַ�
 *
 * @param shell shel����
 * @param data �ַ�
 */
static void shellDisplayByte(SHELL_TypeDef *shell, char data)
{
    if (shell->write == NULL)
    {
        return;
    }
    shell->write(data);
}


#if SHELL_USING_VAR == 1 || SHELL_DISPLAY_RETURN == 1
/**
 * @brief shell��ʾֵ
 *
 * @param shell shell����
 * @param value ֵ
 */
static void shellDisplayValue(SHELL_TypeDef *shell, int value)
{
    char str[11] = "0000000000";
    unsigned int v = value;
    char i = 10;
    char tmp;

    if (value < 0)
    {
        shellDisplay(shell, "-");
        v = -v;
    }
    while (v)
    {
        str[--i] = v % 10 + 48;
        v /= 10;
    }
    shellDisplay(shell, str + i - (value == 0));
    v = value;
    if (value < 0)
    {
        v = (unsigned int)value;
    }
    i = 8;
    str[8] = 0;
    while (v)
    {
        tmp = v & 0x0000000F;
        str[--i] = (tmp > 9) ? (tmp + 87) : (tmp + 48);
        v >>= 4;
    }
    shellDisplay(shell, ", 0x");
    shellDisplay(shell, str);
    shellDisplay(shell, "\r\n");
}
#endif


#if SHELL_DISPLAY_RETURN == 1
/**
 * @brief shell��ʾ�������÷���ֵ
 *
 * @param shell shel����
 * @param value ֵ
 */
static void shellDisplayReturn(SHELL_TypeDef *shell, int value)
{
    shellDisplay(shell, "Return: ");
    shellDisplayValue(shell, value);
}
#endif /** SHELL_DISPLAY_RETURN == 1 */


/**
 * @brief shell�ַ�������
 *
 * @param dest Ŀ���ַ���
 * @param src Դ�ַ���
 * @return unsigned short �ַ�������
 */
static unsigned short shellStringCopy(char *dest, char* src)
{
    unsigned short count = 0;
    while (*(src + count))
    {
        *(dest + count) = *(src + count);
        count++;
    }
    *(dest + count) = 0;
    return count;
}


/**
 * @brief shell�ַ����Ƚ�
 *
 * @param dest Ŀ���ַ���
 * @param src Դ�ַ���
 * @return unsigned short ƥ�䳤��
 */
static unsigned short shellStringCompare(char* dest, char *src)
{
    unsigned short match = 0;
    unsigned short i = 0;

    while (*(dest +i) && *(src + i))
    {
        if (*(dest + i) != *(src +i))
        {
            break;
        }
        match ++;
        i++;
    }
    return match;
}


/**
 * @brief shellɾ��
 *
 * @param shell shell����
 * @param length ɾ���ĳ���
 */
static void shellDelete(SHELL_TypeDef *shell, unsigned short length)
{
    while (length--)
    {
        shellDisplay(shell, "\b \b");
    }
}


/**
 * @brief shell�������
 *
 * @param shell shell����
 */
static void shellClearLine(SHELL_TypeDef *shell)
{
    for (short i = shell->length - shell->cursor; i > 0; i--)
    {
        shellDisplayByte(shell, ' ');
    }
    shellDelete(shell, shell->length);
}


/**
 * @brief shell��ʷ��¼���
 *
 * @param shell shell����
 */
static void shellHistoryAdd(SHELL_TypeDef *shell)
{
    shell->historyOffset = 0;
    if (strcmp(shell->history[shell->historyFlag - 1], shell->buffer) == 0)
    {
        return;
    }
    if (shellStringCopy(shell->history[shell->historyFlag], shell->buffer) != 0)
    {
        shell->historyFlag++;
    }
    if (++shell->historyCount > SHELL_HISTORY_MAX_NUMBER)
    {
        shell->historyCount = SHELL_HISTORY_MAX_NUMBER;
    }
    if (shell->historyFlag >= SHELL_HISTORY_MAX_NUMBER)
    {
        shell->historyFlag = 0;
    }
}


/**
 * @brief shell��ʷ��¼����
 *
 * @param shell shell����
 * @param dir ���ҷ���
 */
static void shellHistory(SHELL_TypeDef *shell, unsigned char dir)
{
#if SHELL_USING_AUTH == 1
    if (!shell->status.authFlag)
    {
        return;
    }
#endif
    if (dir == 0)
    {
        if (shell->historyOffset--
            <= -((shell->historyCount > shell->historyFlag)
            ? shell->historyCount : shell->historyFlag))
        {
            shell->historyOffset = -((shell->historyCount > shell->historyFlag)
                                   ? shell->historyCount : shell->historyFlag);
        }
    }
    else if (dir == 1)
    {
        if (++shell->historyOffset > 0)
        {
            shell->historyOffset = 0;
            return;
        }
    }
    else
    {
        return;
    }
    shellClearLine(shell);
    if (shell->historyOffset == 0)
    {
        shell->cursor = shell->length = 0;
    }
    else
    {
        if ((shell->length = shellStringCopy(shell->buffer,
                shell->history[(shell->historyFlag + SHELL_HISTORY_MAX_NUMBER
                + shell->historyOffset) % SHELL_HISTORY_MAX_NUMBER])) == 0)
        {
            return;
        }
        shell->cursor = shell->length;
        shellDisplay(shell, shell->buffer);
    }
}


/**
 * @brief shell�س����봦��
 *
 * @param shell shell����
 */
static void shellEnter(SHELL_TypeDef *shell)
{
    unsigned char paramCount = 0;
#if !SHELL_QUOTE_KEEP_RAW
    unsigned char quotes = 0;
#endif
    unsigned char record = 1;
    SHELL_CommandTypeDef *base;
    unsigned char runFlag = 0;
    int returnValue;
    (void) returnValue;

#if SHELL_USING_AUTH == 1
    if(shell->status.authFlag == 0)
    {
        if((shell->length != strlen(SHELL_USER_PASSWORD))
            ||(strncmp(shell->buffer, SHELL_USER_PASSWORD, strlen(SHELL_USER_PASSWORD)) != 0))
        {
            shellDisplay(shell, shellText[TEXT_PWD_ERROR]);
            shellDisplay(shell, shellText[TEXT_PWD_HINT]);
        }
        else
        {
            shell->status.authFlag = 1;
            shellDisplay(shell, shellText[TEXT_PWD_RIGHT]);
            shellDisplay(shell, shellText[TEXT_INFO]);
            shellDisplay(shell, shell->command);
        }

        shell->length = 0;
        shell->cursor = 0;
        return;
    }
#endif

    if (shell->length == 0)
    {
        shellDisplay(shell, shell->command);
        return;
    }

    *(shell->buffer + shell->length++) = 0;
    shellHistoryAdd(shell);

    for (unsigned short i = 0; i < shell->length; i++)
    {
        if ((
#if !SHELL_QUOTE_KEEP_RAW
            quotes != 0 ||
#endif
            (*(shell->buffer + i) != ' ' &&
            *(shell->buffer + i) != '\t' &&
            *(shell->buffer + i) != ',')) &&
            *(shell->buffer + i) != 0)
        {
#if !SHELL_QUOTE_KEEP_RAW
            if (*(shell->buffer + i) == '\"')
            {
                quotes = quotes ? 0 : 1;
            #if SHELL_AUTO_PRASE == 0
                *(shell->buffer + i) = 0;
                continue;
            #endif
            }
#endif
            if (record == 1)
            {
                if (paramCount >= SHELL_PARAMETER_MAX_NUMBER)
                    break;
                shell->param[paramCount++] = shell->buffer + i;
                record = 0;
            }
            if (*(shell->buffer + i) == '\\' &&
                *(shell->buffer + i + 1) != 0)
            {
                i++;
            }
        }
        else
        {
            *(shell->buffer + i) = 0;
            record = 1;
        }
    }
    shell->length = 0;
    shell->cursor = 0;
    if (paramCount == 0)
    {
        shellDisplay(shell, shell->command);
        return;
    }

    shellDisplay(shell, "\r\n");
    base = shell->commandBase;
    if (strcmp((const char *)shell->param[0], "help") == 0)
    {
        shell->status.isActive = 1;
        shellHelp(paramCount, shell->param);
        shell->status.isActive = 0;
        shellDisplay(shell, shell->command);
        return;
    }
#if SHELL_USING_VAR == 1
    if (shell->param[0][0] == '$')
    {
        shellDisplayVariable(shell, shell->param[0]);
        shellDisplay(shell, shell->command);
        return;
    }
#endif /** SHELL_USING_VAR == 1 */
    for (unsigned short i = 0; i < shell->commandNumber; i++)
    {
        if (strcmp((const char *)shell->param[0], (base + i)->name) == 0)
        {
            runFlag = 1;
            shell->status.isActive = 1;
        #if SHELL_AUTO_PRASE == 0
            returnValue = (base + i)->function(paramCount, shell->param);
        #else
            returnValue = shellExtRun((base + i)->function, paramCount, shell->param);
        #endif /** SHELL_AUTO_PRASE == 0 */
            shell->status.isActive = 0;
        #if SHELL_DISPLAY_RETURN == 1
            shellDisplayReturn(shell, returnValue);
        #endif /** SHELL_DISPLAY_RETURN == 1 */
            break;
        }
    }
    if (runFlag == 0)
    {
        shellDisplay(shell, shellText[TEXT_CMD_NONE]);
    }
    shellDisplay(shell, shell->command);
}


/**
 * @brief shell�˸����봦��
 *
 * @param shell shell����
 */
static void shellBackspace(SHELL_TypeDef *shell)
{
    if (shell->length == 0)
    {
        return;
    }
    if (shell->cursor == shell->length)
    {
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        shellDelete(shell, 1);
    }
    else if (shell->cursor > 0)
    {
        for (short i = 0; i < shell->length - shell->cursor; i++)
        {
            shell->buffer[shell->cursor + i - 1] = shell->buffer[shell->cursor + i];
        }
        shell->length--;
        shell->cursor--;
        shell->buffer[shell->length] = 0;
        shellDisplayByte(shell, '\b');
        for (short i = shell->cursor; i < shell->length; i++)
        {
            shellDisplayByte(shell, shell->buffer[i]);
        }
        shellDisplayByte(shell, ' ');
        for (short i = shell->length - shell->cursor + 1; i > 0; i--)
        {
            shellDisplayByte(shell, '\b');
        }
    }
}


/**
 * @brief shell Tab�����봦��
 *
 * @param shell shell����
 */
static void shellTab(SHELL_TypeDef *shell)
{
    unsigned short maxMatch = SHELL_COMMAND_MAX_LENGTH;
    unsigned short lastMatchIndex = 0;
    unsigned short matchNum = 0;
    unsigned short length;
    SHELL_CommandTypeDef *base = shell->commandBase;

#if SHELL_USING_AUTH == 1
    if (!shell->status.authFlag)
    {
        return;
    }
#endif

    if (shell->length != 0)
    {
        shell->buffer[shell->length] = 0;
        for (short i = 0; i < shell->commandNumber; i++)
        {
            if (shellStringCompare(shell->buffer,
                (char *)(base + i)->name)
                == shell->length)
            {
                if (matchNum != 0)
                {
                    if (matchNum == 1)
                    {
                        shellDisplay(shell, "\r\n");
                    }
                    shellDisplayItem(shell, lastMatchIndex);
                    length = shellStringCompare((char *)(base + lastMatchIndex)->name,
                                                (char *)(base +i)->name);
                    maxMatch = (maxMatch > length) ? length : maxMatch;
                }
                lastMatchIndex = i;
                matchNum ++;
            }
        }

        if (matchNum == 0)
        {
            return;
        }
        if (matchNum == 1)
        {
            shellClearLine(shell);
        }
        if (matchNum != 0)
        {
            shell->length = shellStringCopy(shell->buffer,
                                            (char *)(base + lastMatchIndex)->name);
        }
        if (matchNum > 1)
        {
            shellDisplayItem(shell, lastMatchIndex);
            shellDisplay(shell, shell->command);
            shell->length = maxMatch;
        }
        shell->buffer[shell->length] = 0;
        shell->cursor = shell->length;
        shellDisplay(shell, shell->buffer);
    }
    else
    {
        shell->status.isActive = 1;
        shellHelp(1, (void *)0);
        shell->status.isActive = 0;
        shellDisplay(shell, shell->command);
    }

#if SHELL_LONG_HELP == 1
    if (SHELL_GET_TICK())
    {
        if (matchNum == 1
            && shell->status.tabFlag == 1
            && SHELL_GET_TICK() - shell->activeTime < SHELL_DOUBLE_CLICK_TIME)
        {
            shellClearLine(shell);
            for (short i = shell->length; i >= 0; i--)
            {
                shell->buffer[i + 5] = shell->buffer[i];
            }
            shellStringCopy(shell->buffer, "help");
            shell->buffer[4] = ' ';
            shell->length += 5;
            shell->cursor = shell->length;
            shellDisplay(shell, shell->buffer);
        }
    }
#endif /** SHELL_LONG_HELP == 1 */
}


/**
 * @brief shell������������
 *
 * @param shell shell����
 * @param data ���������
 */
static void shellNormal(SHELL_TypeDef *shell, char data)
{
    if (data == 0)
    {
        return;
    }
    if (shell->length < SHELL_COMMAND_MAX_LENGTH - 1)
    {
        if (shell->length == shell->cursor)
        {
            shell->buffer[shell->length++] = data;
            shell->cursor++;
            shellDisplayByte(shell, data);
        }
        else
        {
            for (short i = shell->length - shell->cursor; i > 0; i--)
            {
                shell->buffer[shell->cursor + i] = shell->buffer[shell->cursor + i - 1];
            }
            shell->buffer[shell->cursor++] = data;
            shell->buffer[++shell->length] = 0;
            for (short i = shell->cursor - 1; i < shell->length; i++)
            {
                shellDisplayByte(shell, shell->buffer[i]);
            }
            for (short i = shell->length - shell->cursor; i > 0; i--)
            {
                shellDisplayByte(shell, '\b');
            }
        }
    }
    else
    {
        shellDisplay(shell, shellText[TEXT_CMD_TOO_LONG]);
        shellDisplay(shell, shell->command);
        shellDisplay(shell, shell->buffer);
        shell->cursor = shell->length;
    }
}


/**
 * @brief shell��ʼansi��������
 *
 * @param shell shell����
 */
static void shellAnsiStart(SHELL_TypeDef *shell)
{
    shell->status.inputMode = SHELL_ANSI_ESC;
}


/**
 * @brief shell ansi�������д���
 *
 * @param shell shell����
 * @param data ���������
 */
void shellAnsi(SHELL_TypeDef *shell, char data)
{
    switch ((unsigned char)(shell->status.inputMode))
    {
    case SHELL_ANSI_CSI:
        switch (data)
        {
        case 0x41:                                              /** �����ϼ� */
            shellHistory(shell, 0);
            break;

        case 0x42:                                              /** �����¼� */
            shellHistory(shell, 1);
            break;

        case 0x43:                                              /** �����Ҽ� */
            if (shell->cursor < shell->length)
            {
                shellDisplayByte(shell, shell->buffer[shell->cursor]);
                shell->cursor++;
            }
            break;

        case 0x44:                                              /** ������� */
            if (shell->cursor > 0)
            {
                shellDisplayByte(shell, '\b');
                shell->cursor--;
            }
            break;

        default:
            break;
        }
        shell->status.inputMode = SHELL_IN_NORMAL;
        break;

    case SHELL_ANSI_ESC:
        if (data == 0x5B)
        {
            shell->status.inputMode = SHELL_ANSI_CSI;
        }
        else
        {
            shell->status.inputMode = SHELL_IN_NORMAL;
        }
        break;

    default:
        break;
    }
}


/**
 * @brief shell����
 *
 * @param shell shell����
 * @param data ��������
 */
void shellHandler(SHELL_TypeDef *shell, char data)
{
#if SHELL_USING_AUTH == 1 && SHELL_LOCK_TIMEOUT > 0
    if (SHELL_GET_TICK())
    {
        if (SHELL_GET_TICK() - shell->activeTime > SHELL_LOCK_TIMEOUT)
        {
            shell->status.authFlag = 0;
        }
    }
#endif
    if (shell->status.inputMode == SHELL_IN_NORMAL)
    {
        char keyDefFind = 0;
        SHELL_KeyFunctionDef *base = (SHELL_KeyFunctionDef *)shell->keyFuncBase;

    #if SHELL_USING_AUTH == 1
        if (shell->status.authFlag == 1)
        {
    #endif
            for (short i = 0; i < shell->keyFuncNumber; i++)
            {
                if (base[i].keyCode == data) {
                    if (base[i].keyFunction) {
                        base[i].keyFunction(shell);
                    }
                    keyDefFind = 1;
                }
            }
    #if SHELL_USING_AUTH == 1
        }
    #endif
        if (keyDefFind == 0)
        {
            for (short i = 0;
                i < sizeof(shellDefaultKeyFunctionList) / sizeof(SHELL_KeyFunctionDef);
                i++)
            {
                if (shellDefaultKeyFunctionList[i].keyCode == data) {
                    if (shellDefaultKeyFunctionList[i].keyFunction) {
                        shellDefaultKeyFunctionList[i].keyFunction(shell);
                    }
                    keyDefFind = 1;
                }
            }
#if SHELL_FILTER_CTRL_KEYS
            if (keyDefFind == 0)
            {
                if ((data >= SHELL_KEY_CTRL_A) && (data <= SHELL_KEY_CTRL_Z))
                {
                    keyDefFind = 1;
                }
            }
#endif
        }
        if (keyDefFind == 0)
        {
            shellNormal(shell, data);
        }
    }
    else
    {
        shellAnsi(shell, data);
    }
#if SHELL_LONG_HELP == 1 || (SHELL_USING_AUTH && SHELL_LOCK_TIMEOUT > 0)
    if (SHELL_GET_TICK())
    {
        shell->activeTime = SHELL_GET_TICK();
    }
#endif
#if SHELL_LONG_HELP == 1
    shell->status.tabFlag = data == '\t' ? 1 : 0;
#endif
}


#if SHELL_USING_TASK == 1
/**
 * @brief shell ����
 *
 * @param param shell����
 *
 * @note ʹ�ò���ϵͳʱ�������shell read��������������ʽ��
 * @note ��ʹ�ò���ϵͳʱ������ͨ�����ϲ�ѯ�ķ�ʽʹ��shell���޸ĺ�SHELL_TASK_WHILE
 *       Ϊ0��Ȼ������ѭ���в��ϵ��ô˺���
 */
void shellTask(void *param)
{
    SHELL_TypeDef *shell = (SHELL_TypeDef *)param;
    char data;
    if (shell->read == NULL)
    {
        shellDisplay(shell, shellText[TEXT_READ_NOT_DEF]);
        while (1) ;
    }
#if SHELL_TASK_WHILE == 1
    while (1)
    {
#endif
        if (shell->read(&data) == 0)
        {
            shellInput(shell, data);
        }
#if SHELL_TASK_WHILE == 1
    }
#endif
}
#endif


#if SHELL_USING_VAR == 1
/**
 * @brief shell��ȡ����
 *
 * @param shell shell����
 * @param var ����
 * @return int ����ֵ
 */
int shellGetVariable(SHELL_TypeDef *shell, char *var)
{
    SHELL_VaribaleTypeDef *base = shell->variableBase;
    int value = 0;

    if (var[0] == '$')
    {
        var++;
    }

    for (short i = 0; i < shell->variableNumber; i++)
    {
        if (strcmp((const char *)var, (const char *)(base + i)->name) == 0)
        {
            switch ((base + i)->type)
            {
            case SHELL_VAR_INT:
                value = *((int *)((base + i)->value));
                break;
            case SHELL_VAR_SHORT:
                value = *((short *)((base + i)->value));
                break;
            case SHELL_VAR_CHAR:
                value = *((char *)((base + i)->value));
                break;
            case SHELL_VAR_POINTER:
            case SHELL_VAL:
                value = (int)((base + i)->value);
            default:
                break;
            }
        }
    }
    return value;
}


/**
 * @brief shell���ñ���ֵ
 *
 * @param var ����
 * @param value ֵ
 */
void shellSetVariable(char *var, int value)
{
    SHELL_TypeDef *shell = shellGetCurrent();
    char isVarFind = 0;
    if (!shell)
    {
        return;
    }
    SHELL_VaribaleTypeDef *base = shell->variableBase;

    for (short i = 0; i < shell->variableNumber; i++)
    {
        if (strcmp((const char *)var, (const char *)(base + i)->name) == 0)
        {
            switch ((base + i)->type)
            {
            case SHELL_VAR_INT:
                *((int *)((base + i)->value)) = value;
                break;
            case SHELL_VAR_SHORT:
                *((short *)((base + i)->value)) = value;
                break;
            case SHELL_VAR_CHAR:
                *((char *)((base + i)->value)) = value;
                break;
            case SHELL_VAL:
                shellDisplay(shell, "can't set val\r\n");
                break;
            default:
                break;
            }
            isVarFind = 1;
        }
    }
    if (!isVarFind)
    {
        shellDisplay(shell, "var not found\r\n");
    }
    else
    {
        shellDisplayVariable(shell, var);
    }
}
SHELL_EXPORT_CMD_EX(setVar, shellSetVariable, set var, setVar $[var] [value]);


/**
 * @brief shell��ʾ����
 *
 * @param shell shell����
 * @param var ����
 */
static void shellDisplayVariable(SHELL_TypeDef *shell, char *var)
{
    shellDisplay(shell, var[0] == '$' ? var + 1 : var);
    shellDisplay(shell, " = ");
    shellDisplayValue(shell, shellGetVariable(shell, var));
}


/**
 * @brief shell��ʾ���б���
 *
 */
void shellListVariables(void)
{
    SHELL_TypeDef *shell = shellGetCurrent();
    if (!shell)
    {
        return;
    }

    SHELL_VaribaleTypeDef *base = shell->variableBase;

    unsigned short spaceLength;

    shellDisplay(shell, shellText[TEXT_VAR_LIST]);

    for (short i = 0; i <  shell->variableNumber; i++)
    {
        spaceLength = 22 - shellDisplay(shell, (base + i)->name);
        spaceLength = (spaceLength > 0) ? spaceLength : 4;
        do {
            shellDisplay(shell, " ");
        } while (--spaceLength);
        shellDisplay(shell, "--");
        shellDisplay(shell, (base + i)->desc);
        shellDisplay(shell, "\r\n");
    }
}
SHELL_EXPORT_CMD(vars, shellListVariables, show vars);
#endif /** SHELL_USING_VAR == 1 */

/**
 * @brief shell��ʾһ��������Ϣ
 *
 * @param shell shell����
 * @param index Ҫ��ʾ����������
 */
static void shellDisplayItem(SHELL_TypeDef *shell, unsigned short index)
{
    short spaceLength;
    SHELL_CommandTypeDef *base = shell->commandBase;

    spaceLength = 22 - shellDisplay(shell, (base + index)->name);
    spaceLength = (spaceLength > 0) ? spaceLength : 4;
    do {
        shellDisplay(shell, " ");
    } while (--spaceLength);
    shellDisplay(shell, "-- ");
    shellDisplay(shell, (base + index)->desc);
    shellDisplay(shell, "\r\n");

}


/**
 * @brief shell����
 *
 * @param argc ��������
 * @param argv ����
 */
void shellHelp(int argc, char *argv[])
{
    SHELL_TypeDef *shell = shellGetCurrent();
    if (!shell)
    {
        return;
    }
#if SHELL_LONG_HELP == 1
    if (argc == 1)
    {
#endif /** SHELL_LONG_HELP == 1 */
        shellDisplay(shell, shellText[TEXT_FUN_LIST]);
        for(unsigned short i = 0; i < shell->commandNumber; i++)
        {
            shellDisplayItem(shell, i);
        }
#if SHELL_LONG_HELP == 1
    }
    else if (argc == 2) {
        SHELL_CommandTypeDef *base = shell->commandBase;
        for (unsigned char i = 0; i < shell->commandNumber; i++)
        {
            if (strcmp((const char *)argv[1], (base + i)->name) == 0)
            {
                shellDisplay(shell, (base + i)->desc);
                shellDisplay(shell, "\r\n");
                if ((base + i)->help)
                {
                    shellDisplay(shell, "usage:\r\n");
                    shellDisplay(shell, "    ");
                    shellDisplay(shell, (base + i)->help);
                    shellDisplay(shell, "\r\n");
                }
                return;
            }
        }
        shellDisplay(shell, shellText[TEXT_CMD_NONE]);
    }
#endif /** SHELL_LONG_HELP == 1 */
}
SHELL_EXPORT_CMD_EX(help, shellHelp, show help info, help [command] -- show help info of command);


/**
 * @brief ���������
 *
 */
void shellClear(void)
{
    SHELL_TypeDef *shell = shellGetCurrent();
    if (!shell)
    {
        return;
    }
    shellDisplay(shell, "\033[2J\033[1H");
}
SHELL_EXPORT_CMD(cls, shellClear, clear command line);
