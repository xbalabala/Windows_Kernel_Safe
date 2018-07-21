///
/// @file         cf_modify_irp.c
/// @author    crazy_chu wowocock
/// @date       2009-2-1
/// @brief      实现对irp请求和结果的修改 
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
#include "cf_file_irp.h"
#include "cf_list.h"
#include "fat_headers/fat.h"
#include "fat_headers/nodetype.h"
#include "fat_headers/fatstruc.h"

#define CF_FILE_HEADER_SIZE (1024*4)
#define CF_MEM_TAG 'cfmi'

// 对这些set information给予修改，使之隐去前面的4k文件头。
void cfIrpSetInforPre(
    PIRP irp,
    PIO_STACK_LOCATION irpsp)
{
    PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status;

    ASSERT(irpsp->MajorFunction == IRP_MJ_SET_INFORMATION);
    switch(irpsp->Parameters.SetFile.FileInformationClass)
    {
    case FileAllocationInformation:
        {
		    PFILE_ALLOCATION_INFORMATION alloc_infor = 
                (PFILE_ALLOCATION_INFORMATION)buffer;
		    alloc_infor->AllocationSize.QuadPart += CF_FILE_HEADER_SIZE;        
            break;
        }
    case FileEndOfFileInformation:
        {
		    PFILE_END_OF_FILE_INFORMATION end_infor = 
                (PFILE_END_OF_FILE_INFORMATION)buffer;
		    end_infor->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
            break;
        }
    case FileValidDataLengthInformation:
        {
		    PFILE_VALID_DATA_LENGTH_INFORMATION valid_length = 
                (PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
		    valid_length->ValidDataLength.QuadPart += CF_FILE_HEADER_SIZE;
            break;
        }
	case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION position_infor = 
				(PFILE_POSITION_INFORMATION)buffer;
			position_infor->CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
			break;
		}
	case FileStandardInformation:
		((PFILE_STANDARD_INFORMATION)buffer)->EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;
	case FileAllInformation:
		((PFILE_ALL_INFORMATION)buffer)->PositionInformation.CurrentByteOffset.QuadPart += CF_FILE_HEADER_SIZE;
		((PFILE_ALL_INFORMATION)buffer)->StandardInformation.EndOfFile.QuadPart += CF_FILE_HEADER_SIZE;
		break;

    default:
        ASSERT(FALSE);
    };
}

void cfIrpQueryInforPost(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    PUCHAR buffer = irp->AssociatedIrp.SystemBuffer;
    ASSERT(irpsp->MajorFunction == IRP_MJ_QUERY_INFORMATION);
    switch(irpsp->Parameters.QueryFile.FileInformationClass)
    {
    case FileAllInformation:
        {
            // 注意FileAllInformation，是由以下结构组成。即使长度不够，
            // 依然可以返回前面的字节。
            //typedef struct _FILE_ALL_INFORMATION {
            //    FILE_BASIC_INFORMATION BasicInformation;
            //    FILE_STANDARD_INFORMATION StandardInformation;
            //    FILE_INTERNAL_INFORMATION InternalInformation;
            //    FILE_EA_INFORMATION EaInformation;
            //    FILE_ACCESS_INFORMATION AccessInformation;
            //    FILE_POSITION_INFORMATION PositionInformation;
            //    FILE_MODE_INFORMATION ModeInformation;
            //    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
            //    FILE_NAME_INFORMATION NameInformation;
            //} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
            // 我们需要注意的是返回的字节里是否包含了StandardInformation
            // 这个可能影响文件的大小的信息。
            PFILE_ALL_INFORMATION all_infor = (PFILE_ALL_INFORMATION)buffer;
            if(irp->IoStatus.Information >= 
                sizeof(FILE_BASIC_INFORMATION) + 
                sizeof(FILE_STANDARD_INFORMATION))
            {
                ASSERT(all_infor->StandardInformation.EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
				all_infor->StandardInformation.EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
                all_infor->StandardInformation.AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;
                if(irp->IoStatus.Information >= 
                    sizeof(FILE_BASIC_INFORMATION) + 
                    sizeof(FILE_STANDARD_INFORMATION) +
                    sizeof(FILE_INTERNAL_INFORMATION) +
                    sizeof(FILE_EA_INFORMATION) +
                    sizeof(FILE_ACCESS_INFORMATION) +
                    sizeof(FILE_POSITION_INFORMATION))
                {
                    if(all_infor->PositionInformation.CurrentByteOffset.QuadPart >= CF_FILE_HEADER_SIZE)
                        all_infor->PositionInformation.CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
                }
            }
            break;
        }
    case FileAllocationInformation:
        {
		    PFILE_ALLOCATION_INFORMATION alloc_infor = 
                (PFILE_ALLOCATION_INFORMATION)buffer;
            ASSERT(alloc_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
		    alloc_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;        
            break;
        }
    case FileValidDataLengthInformation:
        {
		    PFILE_VALID_DATA_LENGTH_INFORMATION valid_length = 
                (PFILE_VALID_DATA_LENGTH_INFORMATION)buffer;
            ASSERT(valid_length->ValidDataLength.QuadPart >= CF_FILE_HEADER_SIZE);
		    valid_length->ValidDataLength.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
    case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION stand_infor = (PFILE_STANDARD_INFORMATION)buffer;
            ASSERT(stand_infor->AllocationSize.QuadPart >= CF_FILE_HEADER_SIZE);
            stand_infor->AllocationSize.QuadPart -= CF_FILE_HEADER_SIZE;            
            stand_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
    case FileEndOfFileInformation:
        {
		    PFILE_END_OF_FILE_INFORMATION end_infor = 
                (PFILE_END_OF_FILE_INFORMATION)buffer;
            ASSERT(end_infor->EndOfFile.QuadPart >= CF_FILE_HEADER_SIZE);
		    end_infor->EndOfFile.QuadPart -= CF_FILE_HEADER_SIZE;
            break;
        }
	case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION PositionInformation =
				(PFILE_POSITION_INFORMATION)buffer; 
            if(PositionInformation->CurrentByteOffset.QuadPart > CF_FILE_HEADER_SIZE)
			    PositionInformation->CurrentByteOffset.QuadPart -= CF_FILE_HEADER_SIZE;
			break;
		}
    default:
        ASSERT(FALSE);
    };
}

// 读请求。将偏移量前移。
void cfIrpReadPre(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    PLARGE_INTEGER offset;
    PFCB fcb = (PFCB)irpsp->FileObject->FsContext;
	offset = &irpsp->Parameters.Read.ByteOffset;
    if(offset->LowPart ==  FILE_USE_FILE_POINTER_POSITION &&  offset->HighPart == -1)
	{
        // 记事本不会出现这样的情况。
        ASSERT(FALSE);
	}
    // 偏移必须修改为增加4k。
    offset->QuadPart += CF_FILE_HEADER_SIZE;
    KdPrint(("cfIrpReadPre: offset = %8x\r\n",
        offset->LowPart));
}

// 读请求结束，需要解密。
void cfIrpReadPost(PIRP irp,PIO_STACK_LOCATION irpsp)
{
    // 得到缓冲区，然后解密之。解密很简单，就是xor 0x77.
    PUCHAR buffer;
    ULONG i,length = irp->IoStatus.Information;
    ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if(irp->MdlAddress != NULL)
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress,NormalPagePriority);
	else
		buffer = irp->UserBuffer;

    // 解密也很简单，xor 0x77
    for(i=0;i<length;++i)
        buffer[i] ^= 0X77;
    // 打印解密之后的内容
    KdPrint(("cfIrpReadPost: flags = %x length = %x content = %c%c%c%c%c\r\n",
        irp->Flags,length,buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]));
}

// 分配一个MDL，带有一个长度为length的缓冲区。
PMDL cfMdlMemoryAlloc(ULONG length)
{
    void *buf = ExAllocatePoolWithTag(NonPagedPool,length,CF_MEM_TAG);
    PMDL mdl;
    if(buf == NULL)
        return NULL;
    mdl = IoAllocateMdl(buf,length,FALSE,FALSE,NULL);
    if(mdl == NULL)
    {
        ExFreePool(buf);
        return NULL;
    }
    MmBuildMdlForNonPagedPool(mdl);
    mdl->Next = NULL;
    return mdl;
}

// 释放掉带有MDL的缓冲区。
void cfMdlMemoryFree(PMDL mdl)
{
    void *buffer = MmGetSystemAddressForMdlSafe(mdl,NormalPagePriority);
    IoFreeMdl(mdl);
    ExFreePool(buffer);
}

// 写请求上下文。因为写请求必须恢复原来的irp->MdlAddress
// 或者irp->UserBuffer，所以才需要记录上下文。
typedef struct CF_WRITE_CONTEXT_{
    PMDL mdl_address;
    PVOID user_buffer;
} CF_WRITE_CONTEXT,*PCF_WRITE_CONTEXT;

// 写请求需要重新分配缓冲区，而且有可能失败。如果失败
// 了就直接报错了。所以要有一个返回。TRUE表示成功，可
// 以继续GO_ON。FALSE表示失败了，错误已经填好，直接
// 完成即可
BOOLEAN cfIrpWritePre(PIRP irp,PIO_STACK_LOCATION irpsp, PVOID *context)
{
    PLARGE_INTEGER offset;
    ULONG i,length = irpsp->Parameters.Write.Length;
    PUCHAR buffer,new_buffer;
    PMDL new_mdl = NULL;

    // 先准备一个上下文
    PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT)
        ExAllocatePoolWithTag(NonPagedPool,sizeof(CF_WRITE_CONTEXT),CF_MEM_TAG);
    if(my_context == NULL)
    {
        irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        return FALSE;
    }
  
    // 在这里得到缓冲进行加密。要注意的是写请求的缓冲区
    // 是不可以直接改写的。必须重新分配。
    ASSERT(irp->MdlAddress != NULL || irp->UserBuffer != NULL);
	if(irp->MdlAddress != NULL)
    {
		buffer = MmGetSystemAddressForMdlSafe(irp->MdlAddress,NormalPagePriority);
        new_mdl = cfMdlMemoryAlloc(length);
        if(new_mdl == NULL)
            new_buffer = NULL;
        else
            new_buffer = MmGetSystemAddressForMdlSafe(new_mdl,NormalPagePriority);
    }
	else
    {
		buffer = irp->UserBuffer;
        new_buffer = ExAllocatePoolWithTag(NonPagedPool,length,CF_MEM_TAG);
    }
    // 如果缓冲区分配失败了，直接退出即可。
    if(new_buffer == NULL)
    {
        irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        irp->IoStatus.Information = 0;
        ExFreePool(my_context);
        return FALSE;
    }
    RtlCopyMemory(new_buffer,buffer,length);

    // 到了这里一定成功，可以设置上下文了。
    my_context->mdl_address = irp->MdlAddress;
    my_context->user_buffer = irp->UserBuffer;
    *context = (void *)my_context;

    // 给irp指定行的mdl，到完成之后再恢复回来。
    if(new_mdl == NULL)
        irp->UserBuffer = new_buffer;
    else
        irp->MdlAddress = new_mdl;

	offset = &irpsp->Parameters.Write.ByteOffset;
    KdPrint(("cfIrpWritePre: fileobj = %x flags = %x offset = %8x length = %x content = %c%c%c%c%c\r\n",
        irpsp->FileObject,irp->Flags,offset->LowPart,length,buffer[0],buffer[1],buffer[2],buffer[3],buffer[4]));

    // 加密也很简单，xor 0x77
    for(i=0;i<length;++i)
        new_buffer[i] ^= 0x77;

    if(offset->LowPart ==  FILE_USE_FILE_POINTER_POSITION &&  offset->HighPart == -1)
	{
        // 记事本不会出现这样的情况。
        ASSERT(FALSE);
	}
    // 偏移必须修改为增加4KB。
    offset->QuadPart += CF_FILE_HEADER_SIZE;
    return TRUE;
}

// 请注意无论结果如何，都必须进入WritePost.否则会出现无法恢复
// Write的内容，释放已分配的空间的情况。
void cfIrpWritePost(PIRP irp,PIO_STACK_LOCATION irpsp,void *context)
{
    PCF_WRITE_CONTEXT my_context = (PCF_WRITE_CONTEXT) context;
    // 到了这里，可以恢复irp的内容了。
    if(irp->MdlAddress != NULL)
        cfMdlMemoryFree(irp->MdlAddress);
    if(irp->UserBuffer != NULL)
        ExFreePool(irp->UserBuffer);
    irp->MdlAddress = my_context->mdl_address;
    irp->UserBuffer = my_context->user_buffer;
    ExFreePool(my_context);
}
