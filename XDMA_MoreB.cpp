// XDMAdll_V10.cpp : Defines the exported functions for the DLL application.
//
#include "xdmaDLL_public.h"

BYTE* allocate_buffer(size_t size, size_t alignment)
{

	if (size == 0) {
		size = 4;
	}

	if (alignment == 0) {
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);
		alignment = sys_info.dwPageSize;
		//printf("alignment = %d\n",alignment);
	}
	return (BYTE*)_aligned_malloc(size, alignment);
}

void free_buffer(BYTE* buf)
{
	_aligned_free(buf);
}

int write_device(HANDLE device, long address, DWORD size, BYTE *buffer)
{
	DWORD wr_size = 0;
	if (INVALID_SET_FILE_POINTER == SetFilePointer(device, address, NULL, FILE_BEGIN)) {
		fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
		return -3;
	}
	if (!WriteFile(device, (void *)(buffer), (DWORD)(size), &wr_size, NULL))
	{
		return -1;
	}
	if (wr_size != (size))
	{
		return -2;
	}
	return size;
}

int read_device(HANDLE device, long address, DWORD size, BYTE *buffer)
{
	DWORD wr_size = 0;
	if (INVALID_SET_FILE_POINTER == SetFilePointer(device, address, NULL, FILE_BEGIN)) {
		fprintf(stderr, "Error setting file pointer, win32 error code: %ld\n", GetLastError());
		return -3;
	}
	if (!ReadFile(device, (void *)(buffer), (DWORD)(size), &wr_size, NULL))
	{
		return -1;
	}
	// 允许短读：返回本次实际读取字节数，让上层可继续处理已到达数据。
	return static_cast<int>(wr_size);
}

int get_devices(GUID guid, char** devpath, size_t len_devpath)
{

	SP_DEVICE_INTERFACE_DATA device_interface;
	PSP_DEVICE_INTERFACE_DETAIL_DATA dev_detail;
	DWORD index;
	HDEVINFO device_info;
	wchar_t tmp[256];
	device_info = SetupDiGetClassDevs((LPGUID)&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (device_info == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "GetDevices INVALID_HANDLE_VALUE\n");
		exit(-1);
	}

	device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	// enumerate through devices

	for (index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface); ++index) {

		// get required buffer size
		ULONG detailLength = 0;
		if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL) && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get length failed\n");
			break;
		}

		// allocate space for device interface detail
		dev_detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detailLength);
		if (!dev_detail) {
			fprintf(stderr, "HeapAlloc failed\n");
			break;
		}
		dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// get device interface detail
		if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
			fprintf(stderr, "SetupDiGetDeviceInterfaceDetail - get detail failed\n");
			HeapFree(GetProcessHeap(), 0, dev_detail);
			break;
		}
		StringCchCopy(tmp, len_devpath, dev_detail->DevicePath);
		wcstombs(devpath[index], tmp, 256);
		HeapFree(GetProcessHeap(), 0, dev_detail);
	}

	SetupDiDestroyDeviceInfoList(device_info);

	return index;
}

int open_devices(HANDLE *device_hd, DWORD dwAccessPatter, char *device_base_path, const char *device_name)
{
	// get device path from GUID
	char device_path[MAX_PATH + 1] = "";
	wchar_t device_path_w[MAX_PATH + 1];
	strcpy_s(device_path, sizeof device_path, device_base_path);
	strcat_s(device_path, sizeof device_path, "\\");
	strcat_s(device_path, sizeof device_path, device_name);
	// open device file
	mbstowcs(device_path_w, device_path, sizeof(device_path));
	printf("%s\n", device_path);
	*device_hd = CreateFile(device_path_w, dwAccessPatter, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (*device_hd == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Error opening device, win32 error code: %ld\n", GetLastError());
		return 0;
	}
	return 1;
}

int reset_devices(HANDLE device_hd)
{
	unsigned int val = 1;
	int ret = 0;
	if (ret = write_device(device_hd, 0x00, 4, (BYTE*)&val), ret < 0) {
		return ret;
	}
	val = 0;
	if (ret = write_device(device_hd, 0x00, 4, (BYTE*)&val), ret < 0) {
		return ret;
	}
	return 0;
}

int ready_state(HANDLE device_hd, unsigned int *opstate, unsigned int *DDRstate)
{
	int ret = 0;
	if (ret = read_device(device_hd, 0x00, 4, (BYTE*)opstate), ret < 0) {
		return ret;
	}
	if (ret = read_device(device_hd, 0x14, 4, (BYTE*)DDRstate), ret < 0) {
		return ret;
	}
	return ret;
}

int last_packetEn(HANDLE device_hd)
{
	unsigned int val = 1;
	int ret = 0;
	if (ret = write_device(device_hd, 0x1C, 4, (BYTE*)&val), ret < 0) {
		return ret;
	}
	return 0;
}

int last_packetSize(HANDLE device_hd)
{
	unsigned int val = 0;
	int ret = 0;
	if (ret = read_device(device_hd, 0x20, 4, (BYTE*)&val), ret < 0) {
		return ret;
	}
	return val;
}

int GXset_channel(HANDLE device_hd, int ch)
{
	unsigned int val = 0;
	int ret = 0;
	switch (ch)
	{
	case 1:
		val = 0;
		break;
	case 2:
		val = 1;
		break;
	default:
		val = 0;
		break;
	}
	if (ret = write_device(device_hd, 0x24, 4, (BYTE*)&val), ret < 0) {
		return ret;
	}
	return 0;
}
