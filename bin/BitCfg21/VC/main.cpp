/********************************************************************/
/* Configuration Program Rev 1.2                                    */
/* Configuration Program Rev 2.0                                    */
/*   2.0  DESCRIPTION�������؊����Ƃ��� BY .A.USHIRO 2007-12-26 �@�@*/
/*   2.1  �I�����S���͂Ƃ��� BY .A.USHIRO 2008-04-03                */
/*                                                                  */
/********************************************************************/

#include <windows.h>
#include <stdio.h>
#include <conio.h>
//#include <mmsystem.h>
#include <commctrl.h>
#include <SHLWAPI.h>

#include "ftd2xx.h"
#include "resource.h"

#define BUF_SIZE 4096
#define CCLK_PLS 512
#define FILE_NAME_SIZE 512

FT_HANDLE ftHandle;
FT_STATUS ftStatus;
HWND hDlg;

static char cFTDI_DESCRIPTION[256];  // FTDI OPEN����ESCRIPTION

int BitFileOpen( char *path )
{
	OPENFILENAME	fname;
	static char	fn[FILE_NAME_SIZE];
	char		filename[FILE_NAME_SIZE];
	static char	filefilter[] = "�r�b�g�X�g���[��(bit;rbf)\0*.bit;*.rbf\0"
		                       "���ׂẴt�@�C��(*.*)\0*.*\0\0";

	memset( &fname, 0, sizeof(OPENFILENAME) );
	fname.lStructSize    = sizeof(OPENFILENAME);
	fname.hwndOwner      = hDlg;
	fname.lpstrFilter    = filefilter;
	fname.nFilterIndex   = 1;
	fname.lpstrFile      = fn;
	fname.nMaxFile       = sizeof(fn);
	fname.lpstrFileTitle = filename;
	fname.nMaxFileTitle  = sizeof(filename)-1;
	fname.Flags          = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	ZeroMemory( fn, sizeof(fn) );

	if( !GetOpenFileName(&fname) )  return -1;

	memcpy( path, fn, strlen( fn )+1 );

	SendMessage( GetDlgItem(hDlg, IDC_EDIT1), WM_SETTEXT, 0, (LPARAM)path );

	return 0;
}

void FpgaReset()
{
	DWORD dwWritten;
	BYTE prog;
	FT_STATUS ftTxStatus;
	
	ftTxStatus = FT_SetBitMode( ftHandle, (UCHAR)0x02, (UCHAR)1 ); // bit-bang-mode

	if( ftTxStatus != 0x00) {
		SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : reset error!" );
		return ;
	}

	prog = 0xfd;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
	prog = 0xff;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
}

void FpgaConf( char *fname )
{
	FT_STATUS ftTxStatus;
	int i, j;
	int p_size, s_size;
	int  tx_loop, tx_tail;
	BYTE *SerialBuf,*adr_SerialBuf;
	BYTE *ParallelBuf;
	char sbuf[256], strbuf[FILE_NAME_SIZE];
	char extbuf[256];
	DWORD startTime, finishTime, diffTime;
	DWORD readsize, dwWritten;
	HANDLE hFile;
	bool BitXilinx, BitAltera;

	GetDlgItemText( hDlg, IDC_EDIT1, strbuf, FILE_NAME_SIZE );

	//�t�@�C���I�[�v��
	hFile = CreateFile( strbuf, 
	                  GENERIC_READ, 
				      FILE_SHARE_READ, 
					  NULL,
					  OPEN_EXISTING, 
					  NULL,
					  NULL );

	if( hFile == INVALID_HANDLE_VALUE) {
		SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : file error!" );
		return ;
	}

	ZeroMemory( extbuf, 256 );
	memcpy( extbuf, PathFindExtension( strbuf ), 4 ); //�g���q�̃R�s�[
	if( !StrCmpNI( ".bit", extbuf, 4 ) ) {
		BitXilinx = true;
		BitAltera = false;
	}
	else if( !StrCmpNI( ".rbf", extbuf, 4 ) ){
		BitXilinx = false;
		BitAltera = true;
	}
	else {
		return ;
	}

	p_size = GetFileSize( hFile, NULL );     // �t�@�C���T�C�Y
	s_size = (p_size*8*2) + (CCLK_PLS*2);

	ParallelBuf = (BYTE*)malloc(p_size+1); // �̈�m��
	SerialBuf   = (BYTE*)malloc(s_size+1); // �̈�m��
	ZeroMemory( ParallelBuf, p_size+1);    // ������������
	ZeroMemory( SerialBuf,   s_size+1);    // ������������

	startTime = timeGetTime();

	ReadFile( hFile, ParallelBuf, p_size, &readsize, NULL );

	//���v���O���X�o�[�̐ݒ�
	SendMessage( GetDlgItem(hDlg, IDC_PROGRESS1), PBM_SETRANGE32, 0, s_size );

	/* ���ڑ��M�\�ȃf�[�^�ɕϊ����� */
	/* �p������ �� �V���A��           */
	int serial_array = 0;
	BYTE cnv_tmp;
	for( i=0; i<p_size; i++ ) {
		cnv_tmp = ParallelBuf[i];

		if( BitXilinx ) {
			//�U�C�����N�X�̏ꍇ
			for( j=0; j<8; j++ ) {
				if( cnv_tmp & 0x80 ) {
					SerialBuf[serial_array++] = 0x04 | 0xFA;
					SerialBuf[serial_array++] = 0x05 | 0xFA;
				}
				else {
					SerialBuf[serial_array++] = 0x00 | 0xFA;
					SerialBuf[serial_array++] = 0x01 | 0xFA;
				}
				cnv_tmp = cnv_tmp << 1;
			}
		} 
		else if( BitAltera ) {
			//�A���e���̏ꍇ
			for( j=0; j<8; j++ ) {
				if( cnv_tmp & 0x01 ) {
					SerialBuf[serial_array++] = 0x04 | 0xFA;
					SerialBuf[serial_array++] = 0x05 | 0xFA;
				}
				else {
					SerialBuf[serial_array++] = 0x00 | 0xFA;
					SerialBuf[serial_array++] = 0x01 | 0xFA;
				}
				cnv_tmp = cnv_tmp >> 1;
			}
		}

	}

	for( i=0; i<CCLK_PLS; i++ ) {
		SerialBuf[serial_array++] = 0x04 | 0xFA;
		SerialBuf[serial_array++] = 0x05 | 0xFA;
	}


	tx_loop = s_size / BUF_SIZE;             // ��x�ɑ��M�\�ȃT�C�Y�ɕ���
	tx_tail = s_size - (BUF_SIZE * tx_loop); // �c��̃r�b�g�X�g���[��

	ftTxStatus = FT_SetBitMode( ftHandle, (UCHAR)0x07, (UCHAR)1 ); // bit-bang-mode

	BYTE prog = 0xfd;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
	prog = 0xff;
	FT_Write(ftHandle, &prog, 1, &dwWritten);

	int bar= 0;
	adr_SerialBuf = SerialBuf;
	for( i=0; i<tx_loop; i++ ) {
		FT_Write(ftHandle, SerialBuf, BUF_SIZE, &dwWritten);
		SerialBuf = SerialBuf + BUF_SIZE;
			
		SendMessage( GetDlgItem(hDlg, IDC_PROGRESS1), PBM_SETPOS, bar+=BUF_SIZE, 0 );
	}

	if( tx_tail > 0 ) {
		FT_Write(ftHandle, SerialBuf, tx_tail, &dwWritten);
	}

	BYTE done;

	FT_GetBitMode( ftHandle, &done ); // bit-bang-mode
	done = done & 0x08;

	SendMessage( GetDlgItem(hDlg, IDC_PROGRESS1), PBM_SETPOS, s_size, 0 );

	if( done == 0x08 ) {
		// �R���t�B�O���[�V��������
		SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : success" );
	}
	else {
		// �R���t�B�O���[�V�������s
		SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : failure" );
	}


	SerialBuf = adr_SerialBuf;

	finishTime = timeGetTime();
	diffTime = finishTime - startTime;

	sprintf( sbuf, "time : %d msec", diffTime );
	SendMessage( GetDlgItem(hDlg, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)sbuf );

	free( SerialBuf );
	free( ParallelBuf );
	CloseHandle( hFile );	
}

BOOL CALLBACK dlgproc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static char fname[FILE_NAME_SIZE];

	switch(message)
	{
	case WM_INITDIALOG: 
		{


			SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : " );
			SendMessage( GetDlgItem(hDlg, IDC_EDIT3), WM_SETTEXT, 0, (LPARAM)"time : " );
			break;
		}

	case WM_COMMAND: 
		{
			switch( LOWORD(wParam) ) 
			{
			case IDC_BTN_REF:
				BitFileOpen( fname );
				break;
			case IDC_BTN_DWN:

				// �E�B���h�E����/���������ɃX�e�[�^�X���擾
				//2.0 ftStatus = FT_OpenEx("EDA/EDX-002 HuMANDATA LTD.", FT_OPEN_BY_DESCRIPTION, &ftHandle);
				ftStatus = FT_OpenEx(cFTDI_DESCRIPTION, FT_OPEN_BY_DESCRIPTION, &ftHandle);

				if( ftStatus == FT_OK ) {
					FT_ResetDevice( ftHandle );                     // �f�o�C�X���Z�b�g
					FT_Purge( ftHandle, FT_PURGE_RX | FT_PURGE_TX); // �o�b�t�@�N���A
					FT_SetTimeouts( ftHandle, NULL, NULL );         // �^�C���A�E�g�̐ݒ�
					FT_SetLatencyTimer( ftHandle, 100 );            // �^�C�}�̐ݒ�
					FT_SetBaudRate( ftHandle, 1000000 );


					// �R���t�B�M�����[�V����
					FpgaConf( fname );

					FT_Close( ftHandle );
				}
				else {
					//2.0 MessageBox( hDlg, "Can't open FT245BM", "ERROR", MB_ICONWARNING );
					MessageBox( hDlg, "Can't open FT245orFT2232", "ERROR", MB_ICONWARNING );
				}

				break;
			case IDC_BTN_RST:

				// �E�B���h�E����/���������ɃX�e�[�^�X���擾
				//2.0 ftStatus = FT_OpenEx("EDA/EDX-002 HuMANDATA LTD.", FT_OPEN_BY_DESCRIPTION, &ftHandle);
				ftStatus = FT_OpenEx(cFTDI_DESCRIPTION, FT_OPEN_BY_DESCRIPTION, &ftHandle);
				if( ftStatus == FT_OK ) {
					FT_ResetDevice( ftHandle );                     // �f�o�C�X���Z�b�g
					FT_Purge( ftHandle, FT_PURGE_RX | FT_PURGE_TX); // �o�b�t�@�N���A
					FT_SetTimeouts( ftHandle, NULL, NULL );         // �^�C���A�E�g�̐ݒ�
					FT_SetLatencyTimer( ftHandle, 100 );            // �^�C�}�̐ݒ�
					FT_SetBaudRate( ftHandle, 1000000 );

					// ���Z�b�g
					FpgaReset();

					FT_Close( ftHandle );
				}
				else {
					//2.0 MessageBox( hDlg, "Can't open FT245BM", "ERROR", MB_ICONWARNING );
					MessageBox( hDlg, "Can't open FT245orFT2232", "ERROR", MB_ICONWARNING );
				}



				break;
			}
			break;
		}

	case WM_CLOSE:
		{
			PostQuitMessage(0);
			break;
		}
	}

	return false;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int )
{
	MSG msg;
	FT_STATUS ftStatus;

	for(;;){
		sprintf( cFTDI_DESCRIPTION, "EDA/EDX-002 HuMANDATA LTD." ); // try 1
		ftStatus = FT_OpenEx(cFTDI_DESCRIPTION, FT_OPEN_BY_DESCRIPTION, &ftHandle);
		if( ftStatus == FT_OK ) {
			FT_Close( ftHandle );
			break;
		}
		sprintf( cFTDI_DESCRIPTION, "ED-CONFIG HuMANDATA LTD. A" ); //	try 2
		ftStatus = FT_OpenEx(cFTDI_DESCRIPTION, FT_OPEN_BY_DESCRIPTION, &ftHandle);
		if( ftStatus == FT_OK ) {
			FT_Close( ftHandle );
			break;
		}
			break;
	}

////

	hDlg = CreateDialog( hInstance, "DLG_DATA", 0, (DLGPROC)dlgproc );

	if( hDlg != NULL ) {
		ShowWindow( hDlg, SW_SHOW ) ;
		UpdateWindow( hDlg ) ;
	}

    while(GetMessage( &msg, NULL, 0, 0) )
	{
        TranslateMessage( &msg );
		DispatchMessage( &msg );
    }
	// 2008-04-03 by a.ushiro
	ftStatus = FT_OpenEx(cFTDI_DESCRIPTION, FT_OPEN_BY_DESCRIPTION, &ftHandle);
	ftStatus = FT_SetBitMode( ftHandle, (UCHAR)0x00, (UCHAR)1 ); // bit-bang-mode all input
	FT_Close( ftHandle );
	//
	return (int)msg.wParam;
}