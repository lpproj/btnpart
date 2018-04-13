/*
	int86x2.h
	
[License]
	Public Domain.
	Feel free to use...AT YOUR OWN RISK.


[Description and Revision]

	�iLSIC �ȊO�́jint86x ���� bp ���W�X�^�̎󂯓n�����o���Ȃ��̂� 
	������ۂ��̂�����Ă݂��B���ł� far call ������Ă݂��B 

	������
	TURBO C/C++ 1.01
	OpenWatcom 1.2
	DigitalMars C++ 8.40 (with 8.33DOS)
	LSI-C 3.30c (trial)

	2004-05-22	sava
		initial
	2004-05-24	sava
		���ɌƑ��ȕ��@�� LSI-C ���H�łɑΉ�
		�iLSI-C �� int86x �� bp ���W�X�^��������̂ŁAint86x2 ���g���K�v��
		���܂�Ȃ��B����͂����炭 int86x2 �̂ق����x���j
	2002-05-26	sava
		_readflags() �ǉ�


[Reference]

int86x2

�錾
	int int86x2(int vector, union REGS2 *ri, union REGS2 *ro, struct SREGS *sr)

����
	vector
		�Ăяo�����荞�݃x�N�^�ԍ��B
		0 ���� 0xff �܂ŁB��ʃo�C�g�͖��������B
	ri
		���荞�݃T�[�r�X���ɓn�����W�X�^���e�B
	ro
		���荞�݃T�[�r�X����Ԃ���郌�W�X�^���e�B
		�ȑO�̒l��ۑ����Ȃ��Ă������ꍇ�Ari �� ro �͓����|�C���^�ł�
		���܂�Ȃ��B
	sr
		���荞�݃T�[�r�X�ɑ΂��Ď󂯓n�����s�Ȃ��Z�O�����g���W�X�^�̓��e�B
		���荞�݃T�[�r�X�� ds, es ��ύX�����ꍇ�Asr �̓��e���ύX�����B
	
	ri, ro, sr �ɖ����ȃ|�C���^��n�����Ƃ͂ł��Ȃ��B
	

�߂�l
		���荞�݃T�[�r�X���畜�A�����Ƃ��� ax ���W�X�^�̒l�B
		�܂��Aro ���̑S���e�Asr ���� ds, es �G���g���l���ύX�����
		�\��������B
		ro->x.cflag �̓L�����[�t���O�̏�Ԃ�\���B���荞�݃T�[�r�X�����
		���A���ɃL�����[�t���O���Z�b�g����Ă����ꍇ�� 0 �ȊO�̒l�ɂȂ�B
		�L�����[�t���O�����Z�b�g����Ă���� 0 �ɂȂ�B


farcall86x2

�錾
	int farcall86x2(unsigned off, unsigned seg, union REGS2 *ri, union REGS2 *ro, struct SREGS *sr)

����
	off
		far call �G���g���̃I�t�Z�b�g�l 
	seg
		far call �G���g���̃Z�O�����g�l 
	ri, ro, sr
		int86x2 �Ɠ���

�߂�l
	int86x2 �Ɠ���


_readflags

�錾
	unsigned _readflags(void)
	�iinline assembler �̏ꍇ������j

�߂�l
	���݂� flags ���W�X�^�̓��e


*/


#ifndef INT86X2_H__
#define INT86X2_H__

#include <dos.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__WATCOMC__)
#define INT86X2CALL __cdecl
#define INT86X2FAR __far
#define INT86X2NEAR __near
#elif defined(__TURBOC__)
#define INT86X2CALL cdecl
#define INT86X2FAR far
#define INT86X2NEAR near
#elif defined(__SC__) || defined(__DMC__)
#define INT86X2CALL __cdecl
#define INT86X2FAR __far
#define INT86X2NEAR __near
#elif defined(_MSC_VER)
# if _MSC_VER <= 700
# define INT86X2CALL _cdecl
# define INT86X2FAR _far
# define INT86X2NEAR _near
# else
# define INT86X2CALL __cdecl
# define INT86X2FAR __far
# define INT86X2NEAR __near
# endif
#endif


#if defined(__WATCOMC__)
#pragma pack(push, 1)
#elif defined(__SC__) || defined(__DMC__) || defined(_MSC_VER)
#pragma pack(1)
#elif defined(__TURBOC__) || defined(__BORLANDC__)
#pragma option -a-
#endif

struct _WORDREGS2 {
	unsigned short  ax, bx, cx, dx, si, di, cflag, flags, bp;
};

struct _BYTEREGS2 {
	unsigned char  al,ah, bl, bh, cl, ch, dl, dh, sil,sih, dil,dih, cflag,cflagh, flagsl,flagsh, bpl,bph;
};

union REGS2 {
	struct _BYTEREGS2  h;
	struct _WORDREGS2  x;
};

#if defined(__WATCOMC__)
#pragma pack(pop)
#elif defined(__SC__) || defined(__DMC__) || defined(_MSC_VER)
#pragma pack()
#elif defined(__TURBOC__) || defined(__BORLANDC__)
#pragma option -a.
#endif


#if defined(LSI_C)

int  int86x2_nn(void near *, unsigned, unsigned, unsigned, \
		int, union REGS2 near *, union REGS2 near *, struct SREGS near*);
int  int86x2_nf(unsigned, unsigned, unsigned, unsigned, \
		int, union REGS2 far *, union REGS2 far *, struct SREGS far*);
int  farcall86x2_nn(unsigned, unsigned, unsigned, unsigned, \
		unsigned, unsigned , union REGS2 near *, union REGS2 near *, struct SREGS near*);
int  carcall86x2_nf(unsigned, unsigned, unsigned, unsigned, \
		unsigned, unsigned, union REGS2 far *, union REGS2 far *, struct SREGS far*);

#define int86x2(n, r1, r2, sr)	int86x2_nn(0,0,0,0, n, r1, r2, sr)
#define _fint86x2(n, r1, r2, sr)	int86x2_nf(0,0,0,0, n, r1, r2, sr)
#define farcall86x2(o, s, r1, r2, sr)	farcall86x2_nn(0,0,0,0, o, s, r1, r2, sr)
#define _ffarcall86x2(o, s, r1, r2, sr)	farcall86x2_nf(0,0,0,0, o, s, r1, r2, sr)

unsigned _asm_c_readflags(char *);
#define _readflags()	_asm_c_readflags("\n\t" "pushf" "\n\t" "pop\tax" "\n")

#else

int  INT86X2CALL INT86X2NEAR int86x2_nn(int, union REGS2 INT86X2NEAR *, union REGS2 INT86X2NEAR *, struct SREGS INT86X2NEAR *);
int  INT86X2CALL INT86X2NEAR int86x2_nf(int, union REGS2 INT86X2FAR *, union REGS2 INT86X2FAR *, struct SREGS INT86X2FAR *);
int  INT86X2CALL INT86X2FAR int86x2_fn(int, union REGS2 INT86X2NEAR *, union REGS2 INT86X2NEAR *, struct SREGS INT86X2NEAR *);
int  INT86X2CALL INT86X2FAR int86x2_ff(int, union REGS2 INT86X2FAR *, union REGS2 INT86X2FAR *, struct SREGS INT86X2FAR *);

int  INT86X2CALL INT86X2NEAR farcall86x2_nn(unsigned, unsigned, union REGS2 INT86X2NEAR *, union REGS2 INT86X2NEAR *, struct SREGS INT86X2NEAR *);
int  INT86X2CALL INT86X2NEAR farcall86x2_nf(unsigned, unsigned, union REGS2 INT86X2FAR *, union REGS2 INT86X2FAR *, struct SREGS INT86X2FAR *);
int  INT86X2CALL INT86X2FAR farcall86x2_fn(unsigned, unsigned, union REGS2 INT86X2NEAR *, union REGS2 INT86X2NEAR *, struct SREGS INT86X2NEAR *);
int  INT86X2CALL INT86X2FAR farcall86x2_ff(unsigned, unsigned, union REGS2 INT86X2FAR *, union REGS2 INT86X2FAR *, struct SREGS INT86X2FAR *);

unsigned INT86X2CALL INT86X2NEAR _readflags_n(void);
unsigned INT86X2CALL INT86X2FAR _readflags_f(void);

#if defined(__TINY__) || defined(__SMALL__)
#define int86x2(n, r1, r2, sr)	int86x2_nn(n, r1, r2, sr)
#define _fint86x2(n, r1, r2, sr)	int86x2_nf(n, r1, r2, sr)
#define farcall86x2(o, s, r1, r2, sr)	farcall86x2_nn(o, s, r1, r2, sr)
#define _ffarcall86x2(o, s, r1, r2, sr)	farcall86x2_nf(o, s, r1, r2, sr)
#define _readflags	_readflags_n
#elif defined(__MEDIUM__)
#define int86x2(n, r1, r2, sr)	int86x2_fn(n, r1, r2, sr)
#define _fint86x2(n, r1, r2, sr)	int86x2_ff(n, r1, r2, sr)
#define farcall86x2(o, s, r1, r2, sr)	farcall86x2_fn(o, s, r1, r2, sr)
#define _ffarcall86x2(o, s, r1, r2, sr)	farcall86x2_ff(o, s, r1, r2, sr)
#define _readflags	_readflags_f
#elif defined(__COMPACT__)
#define int86x2(n, r1, r2, sr)	int86x2_nf(n, r1, r2, sr)
#define _fint86x2(n, r1, r2, sr)	int86x2_nf(n, r1, r2, sr)
#define farcall86x2(o, s, r1, r2, sr)	farcall86x2_nf(o, s, r1, r2, sr)
#define _ffarcall86x2(o, s, r1, r2, sr)	farcall86x2_nf(o, s, r1, r2, sr)
#define _readflags	_readflags_n
#elif defined(__LARGE__) || defined(__HUGE__)
#define int86x2(n, r1, r2, sr)	int86x2_ff(n, r1, r2, sr)
#define _fint86x2(n, r1, r2, sr)	int86x2_ff(n, r1, r2, sr)
#define farcall86x2(o, s, r1, r2, sr)	farcall86x2_ff(o, s, r1, r2, sr)
#define _ffarcall86x2(o, s, r1, r2, sr)	farcall86x2_ff(o, s, r1, r2, sr)
#define _readflags	_readflags_f
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif
