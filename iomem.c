#include <ntddk.h>
#include <wdf.h>
#include <Wdmsec.h>


VOID IomemIoEvtIoRead (WDFQUEUE Queue, WDFREQUEST Request, size_t Length);
VOID IomemIoEvtIoWrite (WDFQUEUE Queue, WDFREQUEST Request, size_t Length);
NTSTATUS IoMemIoReadWrite(WDFREQUEST Request, size_t Length, ULONG_PTR* copied);		

	
NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath)
{
	WDF_DRIVER_CONFIG config;
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WDFDRIVER driver  = NULL;
	
	PWDFDEVICE_INIT deviceInit = NULL;
	WDFDEVICE device = NULL;
	
	WDF_IO_QUEUE_CONFIG queueConfig;
	WDFQUEUE queue  = NULL;
	
	UNICODE_STRING acl, deviceName;
	
	WDF_DRIVER_CONFIG_INIT(&config, NULL);
	status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &config,&driver);
	if (!NT_SUCCESS(status))
		return STATUS_UNSUCCESSFUL;
	
	///////////////////////////////////////
	
	
	RtlInitUnicodeString(&acl, L"D:P(A;;GA;;;WD)");
	RtlInitUnicodeString(&deviceName, L"\\Device\\iomem_io");

	status = STATUS_UNSUCCESSFUL;
	deviceInit = WdfControlDeviceInitAllocate(driver, (PCUNICODE_STRING)&acl);
	
	if (deviceInit) 
		status = WdfDeviceInitAssignName(deviceInit, (PCUNICODE_STRING)&deviceName);
	
	if (NT_SUCCESS(status))
		status = WdfDeviceCreate(&deviceInit, WDF_NO_OBJECT_ATTRIBUTES, &device);
	
	if (!NT_SUCCESS(status))
	{
		WdfDeviceInitFree(deviceInit);
		return status;
	}
	
	WdfControlFinishInitializing(device);
	
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,  WdfIoQueueDispatchSequential);

    queueConfig.EvtIoRead = IomemIoEvtIoRead;
    queueConfig.EvtIoWrite = IomemIoEvtIoWrite;

	
	
    return WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
}




VOID IomemIoEvtIoRead (WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	ULONG_PTR bytesCopied = 0;
	NTSTATUS status = IoMemIoReadWrite(Request, Length, &bytesCopied);
	
	WdfRequestCompleteWithInformation(Request, status, bytesCopied);
}
	
	
VOID IomemIoEvtIoWrite (WDFQUEUE Queue, WDFREQUEST Request, size_t Length)
{
	ULONG_PTR bytesCopied = 0;
	NTSTATUS status = IoMemIoReadWrite(Request, Length, &bytesCopied);
	
	WdfRequestCompleteWithInformation(Request, status, bytesCopied);	
}
	
	
NTSTATUS IoMemIoReadWrite(WDFREQUEST Request, size_t Length, ULONG_PTR* copied)
{
	NTSTATUS 				status = STATUS_UNSUCCESSFUL;
	WDF_REQUEST_PARAMETERS	params;
	size_t					io_length;
	LONGLONG				port;
	LONG					data;
	PVOID					buffer;
	WDFMEMORY				memory;
	
	WDF_REQUEST_PARAMETERS_INIT(&params);
	WdfRequestGetParameters(Request, &params);
	if (params.Type == WdfRequestTypeRead)
	{
		io_length = params.Parameters.Read.Length;
		port = (LONGLONG)params.Parameters.Read.DeviceOffset;
	}
	else if (params.Type == WdfRequestTypeWrite)
	{
		io_length = params.Parameters.Write.Length;
		port = (LONGLONG)params.Parameters.Write.DeviceOffset;
	}
	else
		return status;
	
	if (io_length > 4 || io_length == 3 || io_length > Length)
		return status;
	
	status = WdfRequestRetrieveOutputMemory(Request, &memory);
	if (!NT_SUCCESS(status))
		return status;

	buffer = WdfMemoryGetBuffer(memory, NULL);
	
	if (params.Type == WdfRequestTypeRead)
	{
		switch (io_length)
		{
			case 1: data = READ_PORT_UCHAR((PUCHAR)port); break;
			case 2: data = READ_PORT_USHORT((PUSHORT)port); break;
			case 4: data = READ_PORT_ULONG((PULONG)port); break;	
			default: return STATUS_UNSUCCESSFUL;
		}
		
		RtlCopyMemory(&data, buffer, io_length);
	}
	else
	{
		RtlCopyMemory(buffer, &data, io_length);
		
		switch (io_length)
		{
			case 1: WRITE_PORT_UCHAR((PUCHAR)port, (UCHAR)data); break;
			case 2: WRITE_PORT_USHORT((PUSHORT)port, (USHORT)data); break;
			case 4: WRITE_PORT_ULONG((PULONG)port, data); break;	
			default: return STATUS_UNSUCCESSFUL;
		}		
		
	}
	
	if (copied)
		*copied = io_length;
	
	return STATUS_SUCCESS;
}