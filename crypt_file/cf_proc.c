///
/// @file         cf_proc.c
/// @author    crazy_chu
/// @date       2009-1-30
/// @brief      获得当前进程的名字。 
/// 
/// 免责声明
/// 本代码为示例代码。未经详尽测试，不保证可靠性。作者对
/// 任何人使用此代码导致的直接和间接损失不负责任。
/// 
/// 授权协议
/// 本代码从属于工程crypt_file.是楚狂人与wowocock为《寒江独
/// 钓——Windows内核编程与信息安全》所编写的文件透明加密
/// 示例。本工程仅仅支持WindowsXP下，FastFat文件系统下记事
/// 本的加密。未测试与杀毒软件或者其他文件过滤驱动并存的
/// 情况。本代码全部权利为作者保留，仅供读者学习和阅读使
/// 用。未经两位作者书面授权，不得直接复制、或者基于此代
/// 码进行修改、利用此代码提供的全部或者部分技术用于商业
/// 的软件开发、或者其他的获利行为。如有违反，作者保留起
/// 诉和获取赔偿之权力。阅读此代码，则自动视为接受以上授
/// 权协议。如不接受此协议者，请不要阅读此代码。
///

#include <ntifs.h>

// 这个函数必须在DriverEntry中调用，否则cfCurProcName将不起作用。
static size_t s_cf_proc_name_offset = 0;
void cfCurProcNameInit()
{
	ULONG i;
	PEPROCESS  curproc;
	curproc = PsGetCurrentProcess();
	// 搜索EPROCESS结构，在其中找到字符串
	for(i=0;i<3*4*1024;i++)
	{
		if(!strncmp("System",(PCHAR)curproc+i,strlen("System"))) 
		{
			s_cf_proc_name_offset = i;
			break;
		}
	}
}

// 以下函数可以获得进程名。返回获得的长度。
ULONG cfCurProcName(PUNICODE_STRING name)
{
	PEPROCESS  curproc;
	ULONG	i,need_len;
    ANSI_STRING ansi_name;
	if(s_cf_proc_name_offset == 0)
		return 0;

    // 获得当前进程PEB,然后移动一个偏移得到进程名所在位置。
	curproc = PsGetCurrentProcess();

    // 这个名字是ansi字符串，现在转化为unicode字符串。
    RtlInitAnsiString(&ansi_name,((PCHAR)curproc + s_cf_proc_name_offset));
    need_len = RtlAnsiStringToUnicodeSize(&ansi_name);
    if(need_len > name->MaximumLength)
    {
        return RtlAnsiStringToUnicodeSize(&ansi_name);
    }
    RtlAnsiStringToUnicodeString(name,&ansi_name,FALSE);
	return need_len;
}

// 判断当前进程是不是notepad.exe
BOOLEAN cfIsCurProcSec(void)
{
    WCHAR name_buf[32] = { 0 };
    UNICODE_STRING proc_name = { 0 };
    UNICODE_STRING note_pad = { 0 };
    ULONG length;
    RtlInitEmptyUnicodeString(&proc_name,name_buf,32*sizeof(WCHAR));
    length = cfCurProcName(&proc_name);
    RtlInitUnicodeString(&note_pad,L"notepad.exe");
    if(RtlCompareUnicodeString(&note_pad,&proc_name,TRUE) == 0)
        return TRUE;
    return FALSE;
}

