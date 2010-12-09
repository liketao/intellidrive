/*
    Control program for a virtual disk driver for Windows NT/2000/XP/Vista.
    Copyright (C) 1999-2008 Bo Brant�n.
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <windows.h>
#include <winioctl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include "intellidrive-device.h"

int FileDiskSyntax(void)
{
    fprintf(stderr, "syntax:\n");
    fprintf(stderr, "intellidrive /mount  <devicenumber> <filename> [size[k|M|G] | /ro | /cd] <drive:>\n");
    fprintf(stderr, "intellidrive /umount <drive:>\n");
    fprintf(stderr, "intellidrive /status <drive:>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "intellidrive formats:\n");
    fprintf(stderr, "  c:\\path\\intellidrive.img\n");
    fprintf(stderr, "  \\Device\\Harddisk0\\Partition1\\path\\intellidrive.img\n");
    fprintf(stderr, "  \\\\server\\share\\path\\intellidrive.img\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "example:\n");
    fprintf(stderr, "intellidrive /mount 0 c:\\intellidrive\\intellidrive.img 512M i:\n");
    fprintf(stderr, "intellidrive /umount i:\n");

    return -1;
}

void PrintLastError(char* Prefix)
{
    LPVOID lpMsgBuf;

    FormatMessage( 
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        0,
        (LPTSTR) &lpMsgBuf,
        0,
        NULL
        );

    fprintf(stderr, "%s %s", Prefix, (LPTSTR) lpMsgBuf);

    LocalFree(lpMsgBuf);
}

int
FileDiskMount(
    int                     DeviceNumber,
    POPEN_FILE_INFORMATION  OpenFileInformation,
    BOOLEAN                 CdImage
)
{
    char    VolumeName[] = "\\\\.\\ :";
    char    DriveName[] = " :\\";
    char    DeviceName[255];
    HANDLE  Device;
    DWORD   BytesReturned;
	FILE* blockMapFile;
	long int sizeOfFile;
	int i;

    VolumeName[4] = OpenFileInformation->DriveLetter;
    DriveName[0] = OpenFileInformation->DriveLetter;

    Device = CreateFile(
        VolumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (Device != INVALID_HANDLE_VALUE)
    {
        SetLastError(ERROR_BUSY);
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    if (CdImage)
    {
        sprintf(DeviceName, DEVICE_NAME_PREFIX "Cd" "%u", DeviceNumber);
    }
    else
    {
        sprintf(DeviceName, DEVICE_NAME_PREFIX "%u", DeviceNumber);
    }

    if (!DefineDosDevice(
        DDD_RAW_TARGET_PATH,
        &VolumeName[4],
        DeviceName
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    Device = CreateFile(
        VolumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (Device == INVALID_HANDLE_VALUE)
    {
        PrintLastError(&VolumeName[4]);
        DefineDosDevice(DDD_REMOVE_DEFINITION, &VolumeName[4], NULL);
        return -1;
    }

    if (!DeviceIoControl(
        Device,
        IOCTL_FILE_DISK_OPEN_FILE,
        OpenFileInformation,
        sizeof(OPEN_FILE_INFORMATION) + OpenFileInformation->FileNameLength - 1,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError("FileDisk:");
        DefineDosDevice(DDD_REMOVE_DEFINITION, &VolumeName[4], NULL);
        return -1;
    }

    SHChangeNotify(SHCNE_DRIVEADD, SHCNF_PATH, DriveName, NULL);

	blockMapFile = fopen("c:\\intellidrive\\blockmap.bin", "rb");

	if( blockMapFile == 0 )
	{
		blockMapFile = fopen("c:\\intellidrive\\blockmap.bin", "wb");	
		fprintf(stderr, "Map File doens't exist, creating a new file\n");
	}
	
	if( blockMapFile != 0 )
	{
		fseek(blockMapFile, 0, SEEK_END);
		sizeOfFile = ftell(blockMapFile) * (4096 / 4);
		if( sizeOfFile != OpenFileInformation->FileSize.QuadPart )
		{
			fprintf(stderr, "size of file (%d) not same as drive size (%d)\n", sizeOfFile, OpenFileInformation->FileSize.QuadPart);
			fclose(blockMapFile);
			blockMapFile = fopen("c:\\intellidrive\\blockmap.bin", "wb");
			for( i = 0; i < OpenFileInformation->FileSize.QuadPart / 4096; i++)
			{
				fwrite(&i, sizeof(i), 1, blockMapFile);
			}
		}
		fclose(blockMapFile);
	}
	else
	{
		fprintf(stderr, "Error: unable to create mapping file\n");
	}

    return 0;
}

int FileDiskUmount(char DriveLetter)
{
    char    VolumeName[] = "\\\\.\\ :";
    char    DriveName[] = " :\\";
    HANDLE  Device;
    DWORD   BytesReturned;

    VolumeName[4] = DriveLetter;
    DriveName[0] = DriveLetter;

    Device = CreateFile(
        VolumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (Device == INVALID_HANDLE_VALUE)
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    if (!DeviceIoControl(
        Device,
        FSCTL_LOCK_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    if (!DeviceIoControl(
        Device,
        IOCTL_FILE_DISK_CLOSE_FILE,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError("FileDisk:");
        return -1;
    }

    if (!DeviceIoControl(
        Device,
        FSCTL_DISMOUNT_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    if (!DeviceIoControl(
        Device,
        FSCTL_UNLOCK_VOLUME,
        NULL,
        0,
        NULL,
        0,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    CloseHandle(Device);

    if (!DefineDosDevice(
        DDD_REMOVE_DEFINITION,
        &VolumeName[4],
        NULL
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    SHChangeNotify(SHCNE_DRIVEREMOVED, SHCNF_PATH, DriveName, NULL);

    return 0;
}

int FileDiskStatus(char DriveLetter)
{
    char                    VolumeName[] = "\\\\.\\ :";
    HANDLE                  Device;
    POPEN_FILE_INFORMATION  OpenFileInformation;
    DWORD                   BytesReturned;

    VolumeName[4] = DriveLetter;

    Device = CreateFile(
        VolumeName,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (Device == INVALID_HANDLE_VALUE)
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    OpenFileInformation = malloc(sizeof(OPEN_FILE_INFORMATION) + MAX_PATH);

    if (!DeviceIoControl(
        Device,
        IOCTL_FILE_DISK_QUERY_FILE,
        NULL,
        0,
        OpenFileInformation,
        sizeof(OPEN_FILE_INFORMATION) + MAX_PATH,
        &BytesReturned,
        NULL
        ))
    {
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    if (BytesReturned < sizeof(OPEN_FILE_INFORMATION))
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        PrintLastError(&VolumeName[4]);
        return -1;
    }

    printf("%c: %.*s %I64u bytes%s\n",
        DriveLetter,
        OpenFileInformation->FileNameLength,
        OpenFileInformation->FileName,
        OpenFileInformation->FileSize,
        OpenFileInformation->ReadOnly ? " ro" : ""
        );

    return 0;
}

int __cdecl main(int argc, char* argv[])
{
    char*                   Command;
    int                     DeviceNumber;
    char*                   FileName;
    char*                   Option;
    char                    DriveLetter;
    BOOLEAN                 CdImage = FALSE;
    POPEN_FILE_INFORMATION  OpenFileInformation;

    Command = argv[1];

    if ((argc == 5 || argc == 6) && !strcmp(Command, "/mount"))
    {
        DeviceNumber = atoi(argv[2]);
        FileName = argv[3];

        if (strlen(FileName) < 2)
        {
            return FileDiskSyntax();
        }

        OpenFileInformation =
            malloc(sizeof(OPEN_FILE_INFORMATION) + strlen(FileName) + 7);

        memset(
            OpenFileInformation,
            0,
            sizeof(OPEN_FILE_INFORMATION) + strlen(FileName) + 7
            );

        if (FileName[0] == '\\')
        {
            if (FileName[1] == '\\')
                // \\server\share\path\filedisk.img
            {
                strcpy(OpenFileInformation->FileName, "\\??\\UNC");
                strcat(OpenFileInformation->FileName, FileName + 1);
            }
            else
                // \Device\Harddisk0\Partition1\path\filedisk.img
            {
                strcpy(OpenFileInformation->FileName, FileName);
            }
        }
        else
            // c:\path\filedisk.img
        {
            strcpy(OpenFileInformation->FileName, "\\??\\");
            strcat(OpenFileInformation->FileName, FileName);
        }

        OpenFileInformation->FileNameLength =
            (USHORT) strlen(OpenFileInformation->FileName);

        if (argc > 5)
        {
            Option = argv[4];
            DriveLetter = argv[5][0];

            if (!strcmp(Option, "/ro"))
            {
                OpenFileInformation->ReadOnly = TRUE;
            }
            else if (!strcmp(Option, "/cd"))
            {
                CdImage = TRUE;
            }
            else
            {
                if (Option[strlen(Option) - 1] == 'G')
                {
                    OpenFileInformation->FileSize.QuadPart =
                        _atoi64(Option) * 1024 * 1024 * 1024;
                }
                else if (Option[strlen(Option) - 1] == 'M')
                {
                    OpenFileInformation->FileSize.QuadPart =
                        _atoi64(Option) * 1024 * 1024;
                }
                else if (Option[strlen(Option) - 1] == 'k')
                {
                    OpenFileInformation->FileSize.QuadPart =
                        _atoi64(Option) * 1024;
                }
                else
                {
                    OpenFileInformation->FileSize.QuadPart =
                        _atoi64(Option);
                }
            }
        }
        else
        {
            DriveLetter = argv[4][0];
        }
        OpenFileInformation->DriveLetter = DriveLetter;
        return FileDiskMount(DeviceNumber, OpenFileInformation, CdImage);
    }
    else if (argc == 3 && !strcmp(Command, "/umount"))
    {
        DriveLetter = argv[2][0];
        return FileDiskUmount(DriveLetter);
    }
    else if (argc == 3 && !strcmp(Command, "/status"))
    {
        DriveLetter = argv[2][0];
        return FileDiskStatus(DriveLetter);
    }
    else
    {
        return FileDiskSyntax();
    }
}
