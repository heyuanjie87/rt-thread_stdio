/*
 *  shell implementation for finsh shell.
 *
 * COPYRIGHT (C) 2006 - 2013, RT-Thread Development Team
 *
 *  This file is part of RT-Thread (http://www.rt-thread.org)
 *  Maintainer: bernard.xiong <bernard.xiong at gmail.com>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2006-04-30     Bernard      the first version for FinSH
 * 2006-05-08     Bernard      change finsh thread stack to 2048
 * 2006-06-03     Bernard      add support for skyeye
 * 2006-09-24     Bernard      remove the code related with hardware
 * 2010-01-18     Bernard      fix down then up key bug.
 * 2010-03-19     Bernard      fix backspace issue and fix device read in shell.
 * 2010-04-01     Bernard      add prompt output when start and remove the empty history
 * 2011-02-23     Bernard      fix variable section end issue of finsh shell
 *                             initialization when use GNU GCC compiler.
 * 2017-01-11     heyuanjie    using stdio
 */

#include <rthw.h>

#include "finsh.h"
#include "shell.h"

#ifdef FINSH_USING_MSH
#include "msh.h"
#endif

#include <stdio.h>

/* finsh thread */
static struct rt_thread finsh_thread;
ALIGN(RT_ALIGN_SIZE)
static char finsh_thread_stack[FINSH_THREAD_STACK_SIZE];

const char *finsh_get_prompt()
{
#define _MSH_PROMPT "msh "
#define _PROMPT     "finsh "
    static char finsh_prompt[RT_CONSOLEBUF_SIZE + 1] = {0};

#ifdef FINSH_USING_MSH
    if (msh_is_used()) strcpy(finsh_prompt, _MSH_PROMPT);
    else
#endif
        strcpy(finsh_prompt, _PROMPT);

#if defined(RT_USING_DFS) && defined(DFS_USING_WORKDIR)
    /* get current working directory */
    getcwd(&finsh_prompt[rt_strlen(finsh_prompt)], RT_CONSOLEBUF_SIZE - rt_strlen(finsh_prompt));
#endif

    strcat(finsh_prompt, ">");

    return finsh_prompt;
}

static void shell_auto_complete(char *prefix)
{
    printf("\n");
#ifdef FINSH_USING_MSH
    if (msh_is_used() == RT_TRUE)
    {
        msh_auto_complete(prefix);
    }
    else
#endif
    {
#ifndef FINSH_USING_MSH_ONLY
        extern void list_prefix(char * prefix);
        list_prefix(prefix);
#endif
    }

    printf("%s%s", FINSH_PROMPT, prefix);
}

#ifndef FINSH_USING_MSH_ONLY
void finsh_run_line(struct finsh_parser *parser, const char *line)
{
    const char *err_str;

    printf("\n");
    finsh_parser_run(parser, (unsigned char *)line);

    /* compile node root */
    if (finsh_errno() == 0)
    {
        finsh_compiler_run(parser->root);
    }
    else
    {
        err_str = finsh_error_string(finsh_errno());
        printf("%s\n", err_str);
    }

    /* run virtual machine */
    if (finsh_errno() == 0)
    {
        char ch;
        finsh_vm_run();

        ch = (unsigned char)finsh_stack_bottom();
        if (ch > 0x20 && ch < 0x7e)
        {
            printf("\t'%c', %d, 0x%08x\n",
                   (unsigned char)finsh_stack_bottom(),
                   (unsigned int)finsh_stack_bottom(),
                   (unsigned int)finsh_stack_bottom());
        }
        else
        {
            printf("\t%d, 0x%08x\n",
                   (unsigned int)finsh_stack_bottom(),
                   (unsigned int)finsh_stack_bottom());
        }
    }

    finsh_flush(parser);
}
#endif

#ifdef FINSH_USING_HISTORY
static rt_bool_t shell_handle_history(struct finsh_shell *shell)
{
#if defined(_WIN32)
    int i;
    printf("\r");

    for (i = 0; i <= 60; i++)
        putchar(' ');
    printf("\r");

#else
    printf("\033[2K\r");
#endif
    printf("%s%s", FINSH_PROMPT, shell->line);
    return RT_FALSE;
}

static void shell_push_history(struct finsh_shell *shell)
{
    if (shell->line_position != 0)
    {
        /* push history */
        if (shell->history_count >= FINSH_HISTORY_LINES)
        {
            /* move history */
            int index;
            for (index = 0; index < FINSH_HISTORY_LINES - 1; index ++)
            {
                memcpy(&shell->cmd_history[index][0],
                       &shell->cmd_history[index + 1][0], FINSH_CMD_SIZE);
            }
            memset(&shell->cmd_history[index][0], 0, FINSH_CMD_SIZE);
            memcpy(&shell->cmd_history[index][0], shell->line, shell->line_position);

            /* it's the maximum history */
            shell->history_count = FINSH_HISTORY_LINES;
        }
        else
        {
            memset(&shell->cmd_history[shell->history_count][0], 0, FINSH_CMD_SIZE);
            memcpy(&shell->cmd_history[shell->history_count][0], shell->line, shell->line_position);

            /* increase count and set current history position */
            shell->history_count ++;
        }
    }
    shell->current_history = shell->history_count;
}
#endif

#ifndef RT_USING_HEAP
struct finsh_shell _shell;
#endif

int finsh_exec(rt_shell_t *shell)
{
    int ch;
    int pmt = 0;

    rt_thread_self()->parameter = shell;

    /* normal is echo mode */
    shell->echo_mode = 1;

#ifndef FINSH_USING_MSH_ONLY
    finsh_init(&shell->parser);
#endif

    printf(FINSH_PROMPT);

    while (1)
    {
        /* read one character from device */
        ch = getchar();

        /*
         * handle control key
         * up key  : 0x1b 0x5b 0x41
         * down key: 0x1b 0x5b 0x42
         * right key:0x1b 0x5b 0x43
         * left key: 0x1b 0x5b 0x44
         */
        if (ch == 0x1b)
        {
            shell->stat = WAIT_SPEC_KEY;
            continue;
        }
        else if (shell->stat == WAIT_SPEC_KEY)
        {
            if (ch == 0x5b)
            {
                shell->stat = WAIT_FUNC_KEY;
                continue;
            }

            shell->stat = WAIT_NORMAL;
        }
        else if (shell->stat == WAIT_FUNC_KEY)
        {
            shell->stat = WAIT_NORMAL;

            if (ch == 0x41) /* up key */
            {
#ifdef FINSH_USING_HISTORY
                /* prev history */
                if (shell->current_history > 0)
                    shell->current_history --;
                else
                {
                    shell->current_history = 0;
                    continue;
                }

                /* copy the history command */
                memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
                       FINSH_CMD_SIZE);
                shell->line_curpos = shell->line_position = strlen(shell->line);
                shell_handle_history(shell);
#endif
                continue;
            }
            else if (ch == 0x42) /* down key */
            {
#ifdef FINSH_USING_HISTORY
                /* next history */
                if (shell->current_history < shell->history_count - 1)
                    shell->current_history ++;
                else
                {
                    /* set to the end of history */
                    if (shell->history_count != 0)
                        shell->current_history = shell->history_count - 1;
                    else
                        continue;
                }

                memcpy(shell->line, &shell->cmd_history[shell->current_history][0],
                       FINSH_CMD_SIZE);
                shell->line_curpos = shell->line_position = strlen(shell->line);
                shell_handle_history(shell);
#endif
                continue;
            }
            else if (ch == 0x44) /* left key */
            {
                if (shell->line_curpos)
                {
                    putchar('\b');
                    shell->line_curpos --;
                }

                continue;
            }
            else if (ch == 0x43) /* right key */
            {
                if (shell->line_curpos < shell->line_position)
                {
                    putchar(shell->line[shell->line_curpos]);
                    shell->line_curpos ++;
                }

                continue;
            }

        }

        /* handle CR key */
        if (ch == '\r')
        {
        }
        /* handle tab key */
        else if (ch == '\t')
        {
            int i;
            /* move the cursor to the beginning of line */
            for (i = 0; i < shell->line_curpos; i++)
                putchar('\b');

            /* auto complete */
            shell_auto_complete(&shell->line[0]);
            /* re-calculate position */
            shell->line_curpos = shell->line_position = strlen(shell->line);

            continue;
        }
        /* handle backspace key */
        else if (ch == 0x7f || ch == 0x08)
        {
            /* note that shell->line_curpos >= 0 */
            if (shell->line_curpos == 0)
                continue;

            shell->line_position--;
            shell->line_curpos--;

            if (shell->line_position > shell->line_curpos)
            {
                int i;

                rt_memmove(&shell->line[shell->line_curpos],
                           &shell->line[shell->line_curpos + 1],
                           shell->line_position - shell->line_curpos);
                shell->line[shell->line_position] = 0;

                printf("\b%s  \b", &shell->line[shell->line_curpos]);

                /* move the cursor to the origin position */
                for (i = shell->line_curpos; i <= shell->line_position; i++)
                    putchar('\b');
            }
            else
            {
                printf("\b \b");
                shell->line[shell->line_position] = 0;
            }

            continue;
        }

        /* handle end of line, break */
        if (ch == '\r' || ch == '\n')
        {
#ifdef FINSH_USING_HISTORY
            shell_push_history(shell);
#endif

#ifdef FINSH_USING_MSH
            if (msh_is_used() == RT_TRUE)
            {
                msh_exec(shell->line, shell->line_position);
            }
            else
#endif
            {
#ifndef FINSH_USING_MSH_ONLY
                /* add ';' and run the command line */
                shell->line[shell->line_position] = ';';

                if (shell->line_position != 0)
                    finsh_run_line(&shell->parser, shell->line);
#endif
            }

			if (pmt == 0 || pmt == ch)
			{
				putchar('\n');
			    printf(FINSH_PROMPT);
				pmt = ch;
			}

            memset(shell->line, 0, sizeof(shell->line));
            shell->line_curpos = shell->line_position = 0;
            continue;
        }

        /* it's a large line, discard it */
        if (shell->line_position >= FINSH_CMD_SIZE)
            shell->line_position = 0;

        /* normal character */
        if (shell->line_curpos < shell->line_position)
        {
            int i;

            rt_memmove(&shell->line[shell->line_curpos + 1],
                       &shell->line[shell->line_curpos],
                       shell->line_position - shell->line_curpos);
            shell->line[shell->line_curpos] = ch;
            if (shell->echo_mode)
                printf("%s", &shell->line[shell->line_curpos]);

            /* move the cursor to new position */
            for (i = shell->line_curpos; i < shell->line_position; i++)
                putchar('\b');
        }
        else
        {
            shell->line[shell->line_position] = ch;
            if (shell->echo_mode)
                putchar(ch);
        }

        ch = 0;
        shell->line_position ++;
        shell->line_curpos++;
        if (shell->line_position >= FINSH_CMD_SIZE)
        {
            /* clear command line */
            shell->line_position = 0;
            shell->line_curpos = 0;
        }
    }

    return 0;
}

static void finsh_thread_entry(void *parameter)
{
    struct finsh_shell *shell;

    shell = (struct finsh_shell *)parameter;

    finsh_exec(shell);
}

void finsh_system_function_init(const void *begin, const void *end)
{
    _syscall_table_begin = (struct finsh_syscall *) begin;
    _syscall_table_end = (struct finsh_syscall *) end;
}

void finsh_system_var_init(const void *begin, const void *end)
{
    _sysvar_table_begin = (struct finsh_sysvar *) begin;
    _sysvar_table_end = (struct finsh_sysvar *) end;
}

#if defined(__ICCARM__) || defined(__ICCRX__)               /* for IAR compiler */
#ifdef FINSH_USING_SYMTAB
#pragma section="FSymTab"
#pragma section="VSymTab"
#endif
#elif defined(__ADSPBLACKFIN__) /* for VisaulDSP++ Compiler*/
#ifdef FINSH_USING_SYMTAB
extern "asm" int __fsymtab_start;
extern "asm" int __fsymtab_end;
extern "asm" int __vsymtab_start;
extern "asm" int __vsymtab_end;
#endif
#elif defined(_MSC_VER)
#pragma section("FSymTab$a", read)
const char __fsym_begin_name[] = "__start";
const char __fsym_begin_desc[] = "begin of finsh";
__declspec(allocate("FSymTab$a")) const struct finsh_syscall __fsym_begin =
{
    __fsym_begin_name,
    __fsym_begin_desc,
    NULL
};

#pragma section("FSymTab$z", read)
const char __fsym_end_name[] = "__end";
const char __fsym_end_desc[] = "end of finsh";
__declspec(allocate("FSymTab$z")) const struct finsh_syscall __fsym_end =
{
    __fsym_end_name,
    __fsym_end_desc,
    NULL
};
#endif

/*
 * @ingroup finsh
 *
 * This function will initialize finsh shell
 */
int finsh_system_init(void)
{
    rt_err_t result;
    struct finsh_shell *shell;

#ifdef FINSH_USING_SYMTAB
#ifdef __CC_ARM                 /* ARM C Compiler */
    extern const int FSymTab$$Base;
    extern const int FSymTab$$Limit;
    extern const int VSymTab$$Base;
    extern const int VSymTab$$Limit;
    finsh_system_function_init(&FSymTab$$Base, &FSymTab$$Limit);
#ifndef FINSH_USING_MSH_ONLY
    finsh_system_var_init(&VSymTab$$Base, &VSymTab$$Limit);
#endif
#elif defined (__ICCARM__) || defined(__ICCRX__)      /* for IAR Compiler */
    finsh_system_function_init(__section_begin("FSymTab"),
                               __section_end("FSymTab"));
    finsh_system_var_init(__section_begin("VSymTab"),
                          __section_end("VSymTab"));
#elif defined (__GNUC__) || defined(__TI_COMPILER_VERSION__)
    /* GNU GCC Compiler and TI CCS */
    extern const int __fsymtab_start;
    extern const int __fsymtab_end;
    extern const int __vsymtab_start;
    extern const int __vsymtab_end;
    finsh_system_function_init(&__fsymtab_start, &__fsymtab_end);
    finsh_system_var_init(&__vsymtab_start, &__vsymtab_end);
#elif defined(__ADSPBLACKFIN__) /* for VisualDSP++ Compiler */
    finsh_system_function_init(&__fsymtab_start, &__fsymtab_end);
    finsh_system_var_init(&__vsymtab_start, &__vsymtab_end);
#elif defined(_MSC_VER)
    unsigned int *ptr_begin, *ptr_end;

    ptr_begin = (unsigned int *)&__fsym_begin;
    ptr_begin += (sizeof(struct finsh_syscall) / sizeof(unsigned int));
    while (*ptr_begin == 0) ptr_begin ++;

    ptr_end = (unsigned int *) &__fsym_end;
    ptr_end --;
    while (*ptr_end == 0) ptr_end --;

    finsh_system_function_init(ptr_begin, ptr_end);
#endif
#endif

    /* create or set shell structure */
#ifdef RT_USING_HEAP
    shell = (struct finsh_shell *)rt_malloc(sizeof(struct finsh_shell));
    if (shell == RT_NULL)
    {
        printf("no memory for shell\n");
        return -1;
    }
#else
    shell = &_shell;
#endif

    memset(shell, 0, sizeof(struct finsh_shell));

    result = rt_thread_init(&finsh_thread,
                            "tshell",
                            finsh_thread_entry, shell,
                            &finsh_thread_stack[0], sizeof(finsh_thread_stack),
                            FINSH_THREAD_PRIORITY, 10);

    if (result == RT_EOK)
        rt_thread_startup(&finsh_thread);

    return 0;
}
INIT_APP_EXPORT(finsh_system_init);
