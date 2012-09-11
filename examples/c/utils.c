#include <stdio.h> /* fprintf */
#include <windows.h> /* FormatMessage */
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include "utils.h"

#if 0
#include "../../include/fscc.h"
#include "\Users\Laboratory\Documents\Visual Studio 2012\Projects\FSCC\FSCC\Public.h"

char* guid_to_string(const GUID* guid, char* str)
{
    sprintf(str, "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
            guid->Data1, guid->Data2, guid->Data3,
            guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
            guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
			 
    return str;
}

HANDLE FsccOpen(DWORD deviceIndex)
{
    HDEVINFO hardwareDeviceInfoSet;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    PSP_INTERFACE_DEVICE_DETAIL_DATA deviceDetail;
    ULONG requiredSize;
    HANDLE deviceHandle = INVALID_HANDLE_VALUE;
    DWORD result;
	DWORD last_error;
	
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	DWORD i;

    //Get a list of devices matching the criteria (hid interface, present)
    hardwareDeviceInfoSet = SetupDiGetClassDevs ((LPGUID)&GUID_DEVINTERFACE_FSCC,
                                                 NULL, // Define no enumerator (global)
                                                 NULL, // Define no
                                                 DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
												); 
	
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;
	   
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    //Go through the list and get the interface data
    result = SetupDiEnumDeviceInterfaces (hardwareDeviceInfoSet,
                                          NULL, //infoData,
                                          (LPGUID)&GUID_DEVINTERFACE_FSCC, //interfaceClassGuid,
                                          deviceIndex, 
                                          &deviceInterfaceData);

    /* Failed to get a device - possibly the index is larger than the number of devices */
    if (result == FALSE)
    {
		last_error = GetLastError();
        SetupDiDestroyDeviceInfoList (hardwareDeviceInfoSet);
		SetLastError(last_error);
		printf("a\n");
        return INVALID_HANDLE_VALUE;
    }

    //Get the details with null values to get the required size of the buffer
    SetupDiGetDeviceInterfaceDetail (hardwareDeviceInfoSet,
                                     &deviceInterfaceData,
                                     NULL, //interfaceDetail,
                                     0, //interfaceDetailSize,
                                     &requiredSize,
                                     0); //infoData))

    //Allocate the buffer
    deviceDetail = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(requiredSize);
    deviceDetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	
    //Fill the buffer with the device details
    if (!SetupDiGetDeviceInterfaceDetail (hardwareDeviceInfoSet,
		&deviceInterfaceData,
		deviceDetail,
		requiredSize,
		&requiredSize,
		NULL)) 
    {
		last_error = GetLastError();
        SetupDiDestroyDeviceInfoList (hardwareDeviceInfoSet);
        free (deviceDetail);
		SetLastError(last_error);
		printf("a\n");
        return INVALID_HANDLE_VALUE;
    }
	
    //Open file on the device
    deviceHandle = CreateFile (deviceDetail->DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,        // no SECURITY_ATTRIBUTES structure
		OPEN_EXISTING, // No special create flags
		FILE_ATTRIBUTE_NORMAL, 
		NULL);       // No template file

	if (deviceHandle == INVALID_HANDLE_VALUE) 
		last_error = GetLastError();
	
    SetupDiDestroyDeviceInfoList (hardwareDeviceInfoSet);
    free (deviceDetail);

	if (deviceHandle == INVALID_HANDLE_VALUE)
		SetLastError(last_error);
		
    return deviceHandle;
}

int test(void)
   {
       HDEVINFO hDevInfo;
       SP_DEVINFO_DATA DeviceInfoData;
       DWORD i;

       // Create a HDEVINFO with all present devices.
//       hDevInfo = SetupDiGetClassDevs(NULL,
//           0, // Enumerator
//           0,
//           DIGCF_PRESENT | DIGCF_ALLCLASSES );
		   
    //Get a list of devices matching the criteria (hid interface, present)
    hDevInfo = SetupDiGetClassDevs ((LPGUID)&GUID_DEVINTERFACE_FSCC,
                                                 NULL, // Define no enumerator (global)
                                                 NULL, // Define no
                                                 DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
												); 
												
       if (hDevInfo == INVALID_HANDLE_VALUE)
       {
           // Insert error handling here.
           return 1;
       }
       
       // Enumerate through all devices in Set.
       
       DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
       for (i=0;SetupDiEnumDeviceInfo(hDevInfo,i,
           &DeviceInfoData);i++)
       {
           DWORD DataT;
           LPTSTR buffer = NULL;
           DWORD buffersize = 0;
           
           //
           // Call function with null to begin with, 
           // then use the returned buffer size (doubled)
           // to Alloc the buffer. Keep calling until
           // success or an unknown failure.
           //
           //  Double the returned buffersize to correct
           //  for underlying legacy CM functions that 
           //  return an incorrect buffersize value on 
           //  DBCS/MBCS systems.
           // 
           while (!SetupDiGetDeviceRegistryProperty(
               hDevInfo,
               &DeviceInfoData,
               SPDRP_DEVICEDESC,
               &DataT,
               (PBYTE)buffer,
               buffersize,
               &buffersize))
           {
               if (GetLastError() == 
                   ERROR_INSUFFICIENT_BUFFER)
               {
                   // Change the buffer size.
                   if (buffer) LocalFree(buffer);
                   // Double the size to avoid problems on 
                   // W2k MBCS systems per KB 888609. 
                   buffer = LocalAlloc(LPTR,buffersize * 2);
               }
               else
               {
                   // Insert error handling here.
                   break;
               }
           }
           
           printf("Result:[%s]\n",buffer);
           
           if (buffer) LocalFree(buffer);
       }
       
       
       if ( GetLastError()!=NO_ERROR &&
            GetLastError()!=ERROR_NO_MORE_ITEMS )
       {
           // Insert error handling here.
           return 1;
       }
       
       //  Cleanup
       SetupDiDestroyDeviceInfoList(hDevInfo);
       
       return 0;
   }
#endif

void DisplayError(LPTSTR s)
{ 
	LPVOID message_buffer;
	DWORD error = GetLastError();
	
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message_buffer,
		0, 
		NULL);
		
	fprintf(stderr, "%s (%i): %s", s, error, (LPCTSTR)message_buffer);

	LocalFree(message_buffer);
}