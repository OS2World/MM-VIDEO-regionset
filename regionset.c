/*
 * regionset: Region Code Manager
 * Copyright (C) 1999 Christian Wolff for convergence integrated media GmbH
 * 1999-12-13
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 * 
 * The author can be reached at scarabaeus@convergence.de, 
 * the project's page is at http://linuxtv.org/dvd/
 */
#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#include <os2.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#define CDROMDISK_EXECMD 0x7A

#define EX_DIRECTION_IN     0x0001
#define EX_PLAYING_CHK      0x0002

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
#pragma pack(1)

typedef struct OS2_ExecSCSICmd
{
    ULONG   ID_code;         // 'CD01'
    USHORT  data_length;     // length of the Data Packet
    USHORT  cmd_length;      // length of the Command Buffer
    USHORT  flags;           // flags
    UCHAR   command[16];     // Command Buffer for SCSI command

} OS2_ExecSCSICmd;

#pragma pack()

#   define GPCMD_READ_DVD_STRUCTURE 0xad
#   define GPCMD_REPORT_KEY         0xa4
#   define GPCMD_SEND_KEY           0xa3
    /* DVD struct types */
#   define DVD_STRUCT_PHYSICAL      0x00
#   define DVD_STRUCT_COPYRIGHT     0x01
#   define DVD_STRUCT_DISCKEY       0x02
#   define DVD_STRUCT_BCA           0x03
#   define DVD_STRUCT_MANUFACT      0x04
    /* Key formats */
#   define DVD_REPORT_AGID          0x00
#   define DVD_REPORT_CHALLENGE     0x01
#   define DVD_SEND_CHALLENGE       0x01
#   define DVD_REPORT_KEY1          0x02
#   define DVD_SEND_KEY2            0x03
#   define DVD_REPORT_TITLE_KEY     0x04
#   define DVD_REPORT_ASF           0x05
#   define DVD_SEND_RPC             0x06
#   define DVD_REPORT_RPC           0x08
#   define DVD_INVALIDATE_AGID      0x3f


#define INIT_SSC( TYPE, SIZE ) \
    struct OS2_ExecSCSICmd sdc; \
    uint8_t p_buffer[ (SIZE) ]; \
    unsigned long ulParamLen; \
    unsigned long ulDataLen; \
    memset( &sdc, 0, sizeof( OS2_ExecSCSICmd ) ); \
    memset( &p_buffer, 0, SIZE ); \
    sdc.data_length = (SIZE); \
    ulParamLen = sizeof(sdc); \
    OS2InitSDC( &sdc, (TYPE) )

/*****************************************************************************
 * OS2InitRDC: initialize a SDC structure for the Execute SCSI-command
 *****************************************************************************
 * This function initializes a OS2 IOCtl 'execute SCSI command' structure
 * for future use, either a read command or a write command.
 *****************************************************************************/

static void OS2InitSDC( OS2_ExecSCSICmd *p_sdc, int i_type )
{

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_sdc->flags = 0;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_sdc->flags = EX_DIRECTION_IN;
            break;
    }

    p_sdc->command[ 0 ] = i_type;
    p_sdc->command[ 8 ] = (p_sdc->data_length >> 8) & 0xff;
    p_sdc->command[ 9 ] =  p_sdc->data_length       & 0xff;
    p_sdc->ID_code         = 0x31304443; // 'CD01'
    p_sdc->cmd_length      = 12;
}

int ioctl_ReportRPC( int i_fd, int *p_type, int *p_mask, int *p_scheme,
                     int *vra,int *ucca)
{
    int i_ret;

    INIT_SSC( GPCMD_REPORT_KEY, 8 );

    sdc.command[ 10 ] = DVD_REPORT_RPC;

    i_ret = DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                        &sdc, sizeof(sdc), &ulParamLen,
                        p_buffer, sizeof(p_buffer), &ulDataLen);

    *vra      = (p_buffer[4] >> 3) & 0x7;
    *ucca     = p_buffer[4] & 0x7;
    *p_type   = p_buffer[4] >> 6;
    *p_mask   = p_buffer[5];
    *p_scheme = p_buffer[6];

    return i_ret;
}

int ioctl_SetRPC( int i_fd, int p_mask)
{
    int i_ret;

    INIT_SSC( GPCMD_SEND_KEY, 8 );

    sdc.command[ 10 ] = DVD_SEND_RPC;
    p_buffer[ 1 ] = 6;
    p_buffer[ 4 ] = p_mask;
    return DosDevIOCtl(i_fd, IOCTL_CDROMDISK, CDROMDISK_EXECMD,
                       &sdc, sizeof(sdc), &ulParamLen,
                       p_buffer, sizeof(p_buffer), &ulDataLen);

}

static int dvd_open ( HFILE *dvd_handle, char *psz_target )
{

    char psz_dvd[7];
    APIRET rc;
    ULONG ulAction;

    sprintf( psz_dvd, "%c:", psz_target[0] );

    rc = DosOpen(psz_dvd,dvd_handle , &ulAction, 0,
                 FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY |
                 OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR, NULL);

    if( rc != NO_ERROR )
        return -1;

    return 0;
}

static int dvd_close ( HFILE dvd_handle )
{
    APIRET rc;
    ULONG ulParamLen;
    ULONG ulDataLen;
    UCHAR ucUnlock;

    // Unlock door
    ucUnlock = 0;
    ulParamLen = 4;

    rc = DosDevIOCtl(dvd_handle, IOCTL_CDROMDISK, CDROMDISK_LOCKUNLOCKDOOR,
                     "CD01", 4, &ulParamLen, &ucUnlock, sizeof(UCHAR), &ulDataLen);

    DosClose( dvd_handle);

    return 0;
}

int main (int argc, char* argv[])
{
    HFILE i_fd;
    int i,err,type,vra,ucca,region_mask,rpc_scheme;
    char ch[3];

    if (argc>1) err=dvd_open(&i_fd, argv[1]);
    else
    {
        printf("Usage: regionset <dvd_drive_letter>");
        return 1;
    }

    if (err<0) {
        printf("ERROR: Could not open drive [%s]\nCheck, is disk in drive\n",argv[1]);
        return 1;
    }

    if (ioctl_ReportRPC(i_fd,&type, &region_mask, &rpc_scheme,
                        &vra,&ucca)) {

        //	if (UDFRPCGet(&type,&vra,&ucca,&region_mask,&rpc_scheme)) {
        printf("ERROR: Could not retreive region code settings of the drive!\n");
    } else {
        printf("Current Region Code settings:\n");
        printf("RPC Phase: %s\n",((rpc_scheme==0)?"Unknown (I)":((rpc_scheme==1)?"II":"Reserved (III?)")));
        printf("type: %s\n",((type==0)?"NONE":((type==1)?"SET":((type==2)?"LAST CHANCE":"PERMANENT"))));
        printf("vendor resets available: %d\n",vra);
        printf("user controlled changes resets available: %d\n",ucca);
        printf("drive plays discs from region(s):");
        for (i=0; i<8; i++) {
            if (!(region_mask&(1<<i))) printf(" %d",i+1);
        }
        printf(", mask=0x%02X\n\n",region_mask);
    }
    printf("Would you like to change the region setting of your drive? [y/n]:");
    fflush(stdout);
    fgets(ch,3,stdin);
    if ((ch[0]=='y') || (ch[0]=='Y')) {
        printf("Enter the new region number for your drive [1..8]:");
        fflush(stdout);
        fgets(ch,3,stdin);
        if ((ch[0]<'0') || (ch[0]>'8')) {
            printf("ERROR: Illegal region code!\n");
            goto ERROR;
        }
        if (ch[0]=='0') i=0;
        else i=~(1<<(ch[0]-'1'));
        if (i==region_mask) {
            printf("Identical region code already set, aborting!\n");
            goto ERROR;
        }
        printf("New mask: 0x%02X, correct? [y/n]:",i);
        fflush(stdout);
        fgets(ch,3,stdin);
        if ((ch[0]=='y') || (ch[0]=='Y')) {
            if (ioctl_SetRPC(i_fd, i)) {
                printf("ERROR: Region code could not be set!\n");
                goto ERROR;
            } else {
                printf("Region code set successfully!\n");
            }
        }
    }
    dvd_close(i_fd);
    return 0;
ERROR:
    dvd_close(i_fd);
    return 1;
}
