/**
 * @file set_folder_alias.c
 * @brief 通过 desktop.ini 设置 Windows 文件夹显示别名的工具
 *
 * 本程序利用 Windows 的 desktop.ini 机制，为指定文件夹设置自定义的显示名称（别名），
 * 而实际路径保持不变。支持 UTF-8 输入，并根据系统 ANSI 代码页自动转换编码。
 *
 * @note 仅适用于 Windows 系统。
 * @author Deepseek
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 将 UTF-8 字符串转换为 ANSI（GBK）编码
 *
 * 该函数通过 Windows API 实现编码转换：先将 UTF-8 转为宽字符（UTF-16），
 * 再将宽字符转为系统当前 ANSI 代码页（通常为 GBK）。
 *
 * @param utf8Str 输入的 UTF-8 字符串（以 null 结尾）
 * @return 指向新分配的 ANSI 字符串的指针，使用后需要调用 free() 释放
 * @note 如果转换失败，返回的字符串可能为空或乱码，调用者需检查
 */
char *Utf8ToAnsi(const char *utf8Str) {
    int wideCharLen = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, NULL, 0);
    if (wideCharLen == 0) return NULL;
    wchar_t *wideStr = (wchar_t *) malloc(wideCharLen * sizeof(wchar_t));
    if (!wideStr) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wideStr, wideCharLen) == 0) {
        free(wideStr);
        return NULL;
    }

    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wideStr, -1, NULL, 0, NULL, NULL);
    char *ansiStr = (char *) malloc(ansiLen);
    if (!ansiStr) {
        free(wideStr);
        return NULL;
    }
    WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, ansiLen, NULL, NULL);

    free(wideStr);
    return ansiStr;
}

/**
 * @brief 根据系统 ANSI 代码页决定是否转换输入字符串
 *
 * 如果系统代码页是 UTF-8（CP_UTF8），则直接复制输入字符串；
 * 否则（如 GBK 等）调用 Utf8ToAnsi() 进行转换，以确保 desktop.ini 内容使用系统默认编码。
 *
 * @param inputStr 输入的 UTF-8 字符串
 * @return 转换后的字符串（可能为原字符串的拷贝或新分配的 ANSI 字符串），需要 free()
 */
char *ConvertToAnsiIfNeeded(const char *inputStr) {
    UINT acp = GetACP();
    if (acp == CP_UTF8) {
        // 系统已经是 UTF-8，无需转换，直接复制
        size_t len = strlen(inputStr) + 1;
        char *result = (char *) malloc(len);
        if (!result) return NULL;
        strcpy(result, inputStr);
        return result;
    } else {
        // 系统是 ANSI（如 GBK），需要从 UTF-8 转换为 ANSI
        return Utf8ToAnsi(inputStr);
    }
}

/**
 * @brief 为目标文件夹设置显示别名（通过创建/修改 desktop.ini）
 *
 * 主要步骤：
 * 1. 检查文件夹是否存在且为目录。
 * 2. 构建 desktop.ini 路径。
 * 3. 若文件已存在，询问用户是否覆盖；若覆盖则删除旧文件。
 * 4. 根据系统编码转换显示名。
 * 5. 写入 [.ShellClassInfo] 和 LocalizedResourceName。
 * 6. 设置 desktop.ini 为隐藏 + 系统属性。
 * 7. 设置文件夹属性为“系统文件夹”（attrib +s +r）。
 *
 * @param folderPath  目标文件夹的完整路径（支持长路径，但建议不超过 MAX_PATH）
 * @param displayName 希望显示的别名（UTF-8 编码）
 * @return 状态码：
 *         - 0: 成功
 *         - 1: 文件夹不存在或不是目录
 *         - 2: 无法创建 desktop.ini 文件（权限不足）
 *         - 3: 用户取消操作（不覆盖现有文件）
 *         - 4: 无法删除现有 desktop.ini 文件（权限不足）
 */
int SetFolderDisplayName(const char *folderPath, const char *displayName) {
    char iniPath[MAX_PATH];
    char cmd[MAX_PATH * 2];
    char *ansiDisplayName = NULL;

    // --- 1. 检查目标文件夹是否存在且为目录 ---
    DWORD attrs = GetFileAttributesA(folderPath);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("错误：指定的路径不存在或不是一个文件夹。\n");
        return 1;
    }

    // --- 2. 构建 desktop.ini 的完整路径 ---
    snprintf(iniPath, sizeof(iniPath), "%s\\desktop.ini", folderPath);

    // --- 3. 检查 desktop.ini 是否已存在，若存在则询问用户 ---
    if (GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES) {
        printf("警告：目标文件夹中已存在 desktop.ini 文件。\n");
        printf("请选择操作：\n");
        printf("1. 覆盖现有文件\n");
        printf("2. 取消操作\n");
        printf("> ");

        int choice;
        scanf("%d", &choice);
        getchar();

        if (choice != 1) {
            printf("操作已取消。\n");
            return 3;
        }

        if (!DeleteFileA(iniPath)) {
            printf("错误：无法删除现有的 desktop.ini 文件。请检查权限。\n");
            return 4;
        }
        printf("已删除现有的 desktop.ini 文件。\n");
    }

    // --- 4. 根据系统编码决定是否转换 displayName ---
    ansiDisplayName = ConvertToAnsiIfNeeded(displayName);
    if (!ansiDisplayName) {
        printf("错误：内存分配失败。\n");
        return 2;
    }

    // --- 5. 创建并写入 desktop.ini 文件 ---
    FILE *f = fopen(iniPath, "w");
    if (!f) {
        printf("错误：无法在目标文件夹中创建 desktop.ini 文件。请检查权限。\n");
        free(ansiDisplayName);
        return 2;
    }
    fprintf(f, "[.ShellClassInfo]\n");
    fprintf(f, "LocalizedResourceName=%s\n", ansiDisplayName);
    fclose(f);

    free(ansiDisplayName);
    ansiDisplayName = NULL;

    // --- 6. 设置 desktop.ini 文件属性：隐藏 + 系统 ---
    SetFileAttributesA(iniPath, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    // --- 7. 设置文件夹属性为“系统文件夹”，使 Windows 读取 desktop.ini ---
    snprintf(cmd, sizeof(cmd), "attrib +s +r \"%s\"", folderPath);
    system(cmd);

    printf("成功！文件夹 '%s' 的显示名称已设置为 '%s'。\n", folderPath, displayName);
    printf("提示：desktop.ini 文件已创建并隐藏。如果名称未立即刷新，可以右击文件夹 属性->自定义->文件夹->还原默认图标按钮 来刷新。\n");
    return 0;
}

/**
 * @brief 主函数：程序入口，提供交互式界面
 *
 * 设置控制台代码页为 UTF-8 以正确显示中文，然后循环获取用户输入的文件夹路径和别名，
 * 调用 SetFolderDisplayName() 进行设置，并询问是否继续。
 *
 * @return 程序退出码（始终返回 0）
 */
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("文件夹别名设置工具 - aliasTool");

    printf("当前系统 ANSI 代码页：%u\n", GetACP());
    printf("当前控制台输入代码页：%u\n", GetConsoleCP());
    printf("当前控制台输出代码页：%u\n", GetConsoleOutputCP());

    char folderPath[MAX_PATH];
    char displayName[256];
    int continueFlag = 1;

    while (continueFlag) {
        printf("========================================\n");
        printf("  文件夹显示别名设置工具 (使用desktop.ini)\n");
        printf("  实际路径不变，仅更改资源管理器显示名\n");
        printf("========================================\n");

        printf("请输入目标文件夹的完整路径:\n> ");
        fgets(folderPath, sizeof(folderPath), stdin);
        folderPath[strcspn(folderPath, "\n")] = 0;

        printf("请输入希望显示的别名 (例如: 我的项目):\n> ");
        fgets(displayName, sizeof(displayName), stdin);
        displayName[strcspn(displayName, "\n")] = 0;

        int result = SetFolderDisplayName(folderPath, displayName);

        switch (result) {
            case 0:
                printf("\n操作已完成！\n");
                break;
            case 1:
                printf("\n错误：指定的路径不存在或不是一个文件夹。\n");
                break;
            case 2:
                printf("\n错误：无法创建 desktop.ini 文件。请检查权限。\n");
                break;
            case 3:
                printf("\n操作已取消。\n");
                break;
            case 4:
                printf("\n错误：无法删除现有 desktop.ini 文件。请检查权限。\n");
                break;
            default:
                printf("\n未知错误。\n");
                break;
        }

        // --- 询问是否继续 ---
        printf("\n是否继续添加别名？(y/n): ");
        char choice;
        scanf(" %c", &choice);
        getchar();

        if (choice != 'y' && choice != 'Y') {
            continueFlag = 0;
        }
    }

    printf("\n程序已退出。按任意键关闭窗口...\n");
    system("pause");

    return 0;
}
