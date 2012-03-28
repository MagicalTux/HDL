#include <stdio.h>
#include <ftd2xx.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define CCLK_PLS 512
#define BUF_SIZE 4096

FT_HANDLE ftHandle;

bool FpgaReset() {
	DWORD dwWritten;
	BYTE prog;
	FT_STATUS ftTxStatus;
	
	ftTxStatus = FT_SetBitMode( ftHandle, (UCHAR)0x02, (UCHAR)1 ); // bit-bang-mode

	if( ftTxStatus != 0x00) {
		return false;
	}

	prog = 0xfd;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
	prog = 0xff;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
	return true;
}

bool FpgaConf( char *fname ) {
	FT_STATUS ftTxStatus;
	int i, j;
	int p_size, s_size;
	int  tx_loop, tx_tail;
	BYTE *SerialBuf,*adr_SerialBuf;
	BYTE *ParallelBuf;
	char extbuf[4];
	DWORD dwWritten;
	int hFile;
	bool BitXilinx, BitAltera;
	struct stat s;

	//ファイルオープン
	hFile = open(fname, O_RDONLY);

	if( hFile == -1) {
		perror("open");
		return false;
	}

	extbuf[3] = 0;
	for(int i = strlen(fname), j = 0; j < 3; j++) extbuf[j] = fname[i-3+j];
	if (strcmp(extbuf, "bit") == 0) {
		BitXilinx = true;
		BitAltera = false;
	} else if(strcmp(extbuf, "rbf") == 0) {
		BitXilinx = false;
		BitAltera = true;
	} else {
		close(hFile);
		return false;
	}

	if (fstat(hFile, &s) == -1) {
		close(hFile);
		return false;
	}

	p_size = s.st_size; // File size
	s_size = (p_size*8*2) + (CCLK_PLS*2);

	ParallelBuf = (BYTE*)malloc(p_size+1); // 領域確保
	SerialBuf   = (BYTE*)malloc(s_size+1); // 領域確保
	memset(ParallelBuf, 0, p_size+1);    // メモリ初期化
	memset(SerialBuf, 0, s_size+1);    // メモリ初期化

	if (read(hFile, ParallelBuf, p_size) != p_size) {
		close(hFile);
		return false;
	}

	//▼プログレスバーの設定
	//SendMessage( GetDlgItem(hDlg, IDC_PROGRESS1), PBM_SETRANGE32, 0, s_size );

	/* 直接送信可能なデータに変換する */
	/* パラレル → シリアル           */
	int serial_array = 0;
	BYTE cnv_tmp;
	for( i=0; i<p_size; i++ ) {
		cnv_tmp = ParallelBuf[i];

		if( BitXilinx ) {
			//ザイリンクスの場合
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
			//アルテラの場合
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


	tx_loop = s_size / BUF_SIZE;             // 一度に送信可能なサイズに分割
	tx_tail = s_size - (BUF_SIZE * tx_loop); // 残りのビットストリーム

	ftTxStatus = FT_SetBitMode( ftHandle, (UCHAR)0x07, (UCHAR)1 ); // bit-bang-mode

	BYTE prog = 0xfd;
	FT_Write(ftHandle, &prog, 1, &dwWritten);
	prog = 0xff;
	FT_Write(ftHandle, &prog, 1, &dwWritten);

	adr_SerialBuf = SerialBuf;
	for( i=0; i<tx_loop; i++ ) {
		FT_Write(ftHandle, SerialBuf, BUF_SIZE, &dwWritten);
		SerialBuf = SerialBuf + BUF_SIZE;
	}

	if( tx_tail > 0 ) {
		FT_Write(ftHandle, SerialBuf, tx_tail, &dwWritten);
	}

	BYTE done;

	FT_GetBitMode( ftHandle, &done ); // bit-bang-mode
	done = done & 0x08;

	//SendMessage( GetDlgItem(hDlg, IDC_PROGRESS1), PBM_SETPOS, s_size, 0 );

	bool success;
	if( done == 0x08 ) {
		// コンフィグレーション成功
		success = true;
		//SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : success" );
	}
	else {
		// コンフィグレーション失敗
		success = false;
		//SendMessage( GetDlgItem(hDlg, IDC_EDIT2), WM_SETTEXT, 0, (LPARAM)"status : failure" );
	}


	SerialBuf = adr_SerialBuf;

	free(SerialBuf);
	free(ParallelBuf);
	close(hFile);

	return success;
}


int main(int argc, char *argv[]) {
	// "EDA/EDX-002 HuMANDATA LTD." or "ED-CONFIG HuMANDATA LTD. A"
	FT_STATUS ftStatus;

	FT_SetVIDPID(0x0f87, 0x1004);
	ftStatus = FT_OpenEx("EDA/EDX-002 HuMANDATA LTD.", FT_OPEN_BY_DESCRIPTION, &ftHandle);
	if (ftStatus != FT_OK) {
		fprintf(stderr, "Failed to locate FPGA, please check USB link\n");
		return 1;
	}
	fprintf(stderr, "Resetting...\n");
	FT_ResetDevice(ftHandle);
	FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
	FT_SetTimeouts(ftHandle, 0, 0);
	FT_SetLatencyTimer(ftHandle, 100);
	FT_SetBaudRate(ftHandle, 1000000);

	// check argc
	if (argc == 1) {
		FT_Close(ftHandle);
		return 0;
	}
	fprintf(stderr, "Programming...\n");
	if (!FpgaConf(argv[1])) {
		fprintf(stderr, "Programming failed\n");
	}
	FT_Close(ftHandle);
	return 0;
}

