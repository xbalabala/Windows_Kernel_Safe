///
/// @file         cf_file_irp.c
/// @author    crazy_chu
/// @date       2009-1-29
/// @brief       对文件的操作，直接发送irp来避免重入
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

#define CF_MEM_TAG 'cffi'

static NTSTATUS cfFileIrpComp(
    PDEVICE_OBJECT dev,
    PIRP irp,
    PVOID context
    )
{
    *irp->UserIosb = irp->IoStatus;
    KeSetEvent(irp->UserEvent, 0, FALSE);
    IoFreeIrp(irp);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

// 自发送SetInformation请求.
NTSTATUS 
cfFileSetInformation( 
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    FILE_INFORMATION_CLASS infor_class,
	FILE_OBJECT *set_file,
    void* buf,
    ULONG buf_len)
{
    PIRP irp;
    KEVENT event;
    IO_STATUS_BLOCK IoStatusBlock;
    PIO_STACK_LOCATION ioStackLocation;

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

	// 填写irp的主体
    irp->AssociatedIrp.SystemBuffer = buf;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
    irp->Flags = 0;

	// 设置irpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
    ioStackLocation->MajorFunction = IRP_MJ_SET_INFORMATION;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
    ioStackLocation->Parameters.SetFile.FileObject = set_file;
    ioStackLocation->Parameters.SetFile.Length = buf_len;
    ioStackLocation->Parameters.SetFile.FileInformationClass = infor_class;

	// 设置结束例程
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// 发送请求并等待结束
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
    return IoStatusBlock.Status;
}

NTSTATUS
cfFileQueryInformation(
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    FILE_INFORMATION_CLASS infor_class,
    void* buf,
    ULONG buf_len)
{
    PIRP irp;
    KEVENT event;
    IO_STATUS_BLOCK IoStatusBlock;
    PIO_STACK_LOCATION ioStackLocation;

    // 因为我们打算让这个请求同步完成，所以初始化一个事件
    // 用来等待请求完成。
    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

	// 填写irp的主体
    irp->AssociatedIrp.SystemBuffer = buf;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
    irp->Flags = 0;

	// 设置irpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
    ioStackLocation->MajorFunction = IRP_MJ_QUERY_INFORMATION;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
    ioStackLocation->Parameters.QueryFile.Length = buf_len;
    ioStackLocation->Parameters.QueryFile.FileInformationClass = infor_class;

	// 设置结束例程
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);

	// 发送请求并等待结束
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
    return IoStatusBlock.Status;
}

NTSTATUS
cfFileSetFileSize(
	DEVICE_OBJECT *dev,
	FILE_OBJECT *file,
	LARGE_INTEGER *file_size)
{
	FILE_END_OF_FILE_INFORMATION end_of_file;
	end_of_file.EndOfFile.QuadPart = file_size->QuadPart;
	return cfFileSetInformation(
		dev,file,FileEndOfFileInformation,
		NULL,(void *)&end_of_file,
		sizeof(FILE_END_OF_FILE_INFORMATION));
}

NTSTATUS
cfFileGetStandInfo(
	PDEVICE_OBJECT dev,
	PFILE_OBJECT file,
	PLARGE_INTEGER allocate_size,
	PLARGE_INTEGER file_size,
	BOOLEAN *dir)
{
	NTSTATUS status;
	PFILE_STANDARD_INFORMATION infor = NULL;
	infor = (PFILE_STANDARD_INFORMATION)
		ExAllocatePoolWithTag(NonPagedPool,sizeof(FILE_STANDARD_INFORMATION),CF_MEM_TAG);
	if(infor == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	status = cfFileQueryInformation(dev,file,
		FileStandardInformation,(void *)infor,
		sizeof(FILE_STANDARD_INFORMATION));
	if(NT_SUCCESS(status))
	{
		if(allocate_size != NULL)
			*allocate_size = infor->AllocationSize;
		if(file_size != NULL)
			*file_size = infor->EndOfFile;
		if(dir != NULL)
			*dir = infor->Directory;
	}
	ExFreePool(infor);
	return status;
}


NTSTATUS 
cfFileReadWrite( 
    DEVICE_OBJECT *dev, 
    FILE_OBJECT *file,
    LARGE_INTEGER *offset,
    ULONG *length,
    void *buffer,
    BOOLEAN read_write) 
{
	ULONG i;
    PIRP irp;
    KEVENT event;
    PIO_STACK_LOCATION ioStackLocation;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

	// 分配irp.
    irp = IoAllocateIrp(dev->StackSize, FALSE);
    if(irp == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
  
	// 填写主体。
    irp->AssociatedIrp.SystemBuffer = NULL;
	// 在paging io的情况下，似乎必须要使用MDL才能正常进行。不能使用UserBuffer.
	// 但是我并不肯定这一点。所以这里加一个断言。以便我可以跟踪错误。
    irp->MdlAddress = NULL;
    irp->UserBuffer = buffer;
    irp->UserEvent = &event;
    irp->UserIosb = &IoStatusBlock;
    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = file;
    irp->RequestorMode = KernelMode;
	if(read_write)
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_READ_OPERATION|IRP_NOCACHE;
	else
		irp->Flags = IRP_DEFER_IO_COMPLETION|IRP_WRITE_OPERATION|IRP_NOCACHE;

	// 填写irpsp
    ioStackLocation = IoGetNextIrpStackLocation(irp);
	if(read_write)
		ioStackLocation->MajorFunction = IRP_MJ_READ;
	else
		ioStackLocation->MajorFunction = IRP_MJ_WRITE;
    ioStackLocation->MinorFunction = IRP_MN_NORMAL;
    ioStackLocation->DeviceObject = dev;
    ioStackLocation->FileObject = file;
	if(read_write)
	{
		ioStackLocation->Parameters.Read.ByteOffset = *offset;
		ioStackLocation->Parameters.Read.Length = *length;
	}
	else
	{
		ioStackLocation->Parameters.Write.ByteOffset = *offset;
		ioStackLocation->Parameters.Write.Length = *length;
	}

	// 设置完成
    IoSetCompletionRoutine(irp, cfFileIrpComp, 0, TRUE, TRUE, TRUE);
    (void) IoCallDriver(dev, irp);
    KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);
	*length = IoStatusBlock.Information;
    return IoStatusBlock.Status;
}

// 清理缓冲
void cfFileCacheClear(PFILE_OBJECT pFileObject)
{
   PFSRTL_COMMON_FCB_HEADER pFcb;
   LARGE_INTEGER liInterval;
   BOOLEAN bNeedReleaseResource = FALSE;
   BOOLEAN bNeedReleasePagingIoResource = FALSE;
   KIRQL irql;

   pFcb = (PFSRTL_COMMON_FCB_HEADER)pFileObject->FsContext;
   if(pFcb == NULL)
       return;

   irql = KeGetCurrentIrql();
   if (irql >= DISPATCH_LEVEL)
   {
       return;
   }

   liInterval.QuadPart = -1 * (LONGLONG)50;

   while (TRUE)
   {
       BOOLEAN bBreak = TRUE;
       BOOLEAN bLockedResource = FALSE;
       BOOLEAN bLockedPagingIoResource = FALSE;
       bNeedReleaseResource = FALSE;
       bNeedReleasePagingIoResource = FALSE;

	   // 到fcb中去拿锁。
       if (pFcb->PagingIoResource)
           bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);

	   // 总之一定要拿到这个锁。
       if (pFcb->Resource)
       {
           bLockedResource = TRUE;
           if (ExIsResourceAcquiredExclusiveLite(pFcb->Resource) == FALSE)
           {
               bNeedReleaseResource = TRUE;
               if (bLockedPagingIoResource)
               {
                   if (ExAcquireResourceExclusiveLite(pFcb->Resource, FALSE) == FALSE)
                   {
                       bBreak = FALSE;
                       bNeedReleaseResource = FALSE;
                       bLockedResource = FALSE;
                   }
               }
               else
                   ExAcquireResourceExclusiveLite(pFcb->Resource, TRUE);
           }
       }
   
       if (bLockedPagingIoResource == FALSE)
       {
           if (pFcb->PagingIoResource)
           {
               bLockedPagingIoResource = TRUE;
               bNeedReleasePagingIoResource = TRUE;
               if (bLockedResource)
               {
                   if (ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, FALSE) == FALSE)
                   {
                       bBreak = FALSE;
                       bLockedPagingIoResource = FALSE;
                       bNeedReleasePagingIoResource = FALSE;
                   }
               }
               else
               {
                   ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, TRUE);
               }
           }
       }

       if (bBreak)
       {
           break;
       }
       
       if (bNeedReleasePagingIoResource)
       {
           ExReleaseResourceLite(pFcb->PagingIoResource);
       }
       if (bNeedReleaseResource)
       {
           ExReleaseResourceLite(pFcb->Resource);
       }

       if (irql == PASSIVE_LEVEL)
       {
           KeDelayExecutionThread(KernelMode, FALSE, &liInterval);
       }
       else
       {
           KEVENT waitEvent;
           KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
           KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, &liInterval);
       }
   }

   if (pFileObject->SectionObjectPointer)
   {
		IO_STATUS_BLOCK ioStatus;
		CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &ioStatus);
		if (pFileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(pFileObject->SectionObjectPointer,MmFlushForWrite); // MmFlushForDelete
		}
		CcPurgeCacheSection(pFileObject->SectionObjectPointer, NULL, 0, FALSE);
   }

   if (bNeedReleasePagingIoResource)
   {
       ExReleaseResourceLite(pFcb->PagingIoResource);
   }
   if (bNeedReleaseResource)
   {
       ExReleaseResourceLite(pFcb->Resource);
   }
}

