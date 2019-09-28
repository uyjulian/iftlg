 //---------------------------------------------------------------------------

#include "tlg.h"
#include "slide.h"

#include "TLG6BS.h"

extern tTJSBinaryStream *GetMemoryStream();

// Table for 'k' (predicted bit length) of golomb encoding
// tvpgl.c にあるものを参照
#define TVP_TLG6_GOLOMB_N_COUNT 4
extern "C" char TVPTLG6GolombBitLengthTable[TVP_TLG6_GOLOMB_N_COUNT*2*128][TVP_TLG6_GOLOMB_N_COUNT];
extern "C" void TVPCreateTable(void);

#define MAX_COLOR_COMPONENTS 4

#define FILTER_TRY_COUNT 16

#define W_BLOCK_SIZE 8
#define H_BLOCK_SIZE 8

//------------------------------ FOR DEBUG
//#define FILTER_TEST
//#define WRITE_ENTROPY_VALUES
//#define WRITE_VSTXT
//------------------------------


/*
	shift-jis

	TLG6 圧縮アルゴリズム(概要)

	TLG6 は吉里吉里２で用いられる画像圧縮フォーマットです。グレースケール、
	24bitビットマップ、8bitアルファチャンネル付き24bitビットマップに対応し
	ています。デザインのコンセプトは「高圧縮率であること」「画像展開において
	十分に高速な実装ができること」です。

	TLG6 は以下の順序で画像を圧縮します。

	1.   MED または 平均法 による輝度予測
	2.   リオーダリングによる情報量削減
	3.   色相関フィルタによる情報量削減
	4.   ゴロム・ライス符号によるエントロピー符号化



	1.   MED または 平均法 による輝度予測

		TLG6 は MED (Median Edge Detector) あるいは平均法による輝度予測を行
		い、予測値と実際の値の誤差を記録します。

		MED は、予測対象とするピクセル位置(下図x位置)に対し、左隣のピクセル
		の輝度をa、上隣のピクセルの輝度をb、左上のピクセルの輝度をcとし、予
		測対象のピクセルの輝度 x を以下のように予測します。

			+-----+-----+
			|  c  |  b  |
			+-----+-----+
			|  a  |  x  |
			+-----+-----+

			x = min(a,b)    (c > max(a,b) の場合)
			x = max(a,b)    (c < min(a,b) の場合)
			x = a + b - c   (その他の場合)

		MEDは、単純な、周囲のピクセルの輝度の平均による予測などと比べ、画像
		の縦方向のエッジでは上のピクセルを、横方向のエッジでは左のピクセルを
		参照して予測を行うようになるため、効率のよい予測が可能です。

		平均法では、a と b の輝度の平均から予測対象のピクセルの輝度 x を予
		測します。以下の式を用います。

			x = (a + b + 1) >> 1

		MED と 平均法は、8x8 に区切られた画像ブロック内で両方による圧縮率を
		比較し、圧縮率の高かった方の方法が選択されます。

		R G B 画像や A R G B 画像に対してはこれを色コンポーネントごとに行い
		ます。

	2.   リオーダリングによる情報量削減

		誤差のみとなったデータは、リオーダリング(並び替え)が行われます。
		TLG6では、まず画像を8×8の小さなブロックに分割します。ブロックは横
		方向に、左のブロックから順に右のブロックに向かい、一列分のブロック
		の処理が完了すれば、その一列下のブロック、という方向で処理を行いま
		す。
		その個々のブロック内では、ピクセルを以下の順番に並び替えます。

		横位置が偶数のブロック      横位置が奇数のブロック
		 1  2  3  4  5  6  7  8     57 58 59 60 61 62 63 64
		16 15 14 13 12 11 10  9     56 55 54 53 52 51 50 49
		17 18 19 20 21 22 23 24     41 42 43 44 45 46 47 48
		32 31 30 29 28 27 26 25     40 39 38 37 36 35 34 33
		33 34 35 36 37 38 39 40     25 26 27 28 29 30 31 32
		48 47 46 45 44 43 42 41     24 23 22 21 20 19 18 17
		49 50 51 52 53 54 55 56      9 10 11 12 13 14 15 16
		64 63 62 61 60 59 58 57      8  7  6  5  4  3  2  1

		このような「ジグザグ」状の並び替えを行うことにより、性質(輝度や色相、
		エッジなど)の似たピクセルが連続する可能性が高くなり、後段のエントロ
		ピー符号化段でのゼロ・ラン・レングス圧縮の効果を高めることができます。

	3.   色相関フィルタによる情報量削減

		R G B 画像 ( A R G B 画像も同じく ) においては、色コンポーネント間
		の相関性を利用して情報量を減らせる可能性があります。TLG6では、以下の
		16種類のフィルタをブロックごとに適用し、後段のゴロム・ライス符号によ
		る圧縮後サイズを試算し、もっともサイズの小さくなるフィルタを適用しま
		す。フィルタ番号と MED/平均法の別は LZSS 法により圧縮され、出力され
		ます。

		0          B        G        R              (フィルタ無し)
		1          B-G      G        R-G
		2          B        G-B      R-B-G
		3          B-R-G    G-R      R
		4          B-R      G-B-R    R-B-R-G
		5          B-R      G-B-R    R
		6          B-G      G        R
		7          B        G-B      R
		8          B        G        R-G
		9          B-G-R-B  G-R-B    R-B
		10         B-R      G-R      R
		11         B        G-B      R-B
		12         B        G-R-B    R-B
		13         B-G      G-R-B-G  R-B-G
		14         B-G-R    G-R      R-B-G-R
		15         B        G-(B<<1) R-(B<<1)

		これらのフィルタは、多くの画像でテストした中、フィルタの使用頻度を調
		べ、もっとも適用の多かった16個のフィルタを選出しました。

	4.   ゴロム・ライス符号によるエントロピー符号化

		最終的な値はゴロム・ライス符号によりビット配列に格納されます。しかし
		最終的な値には連続するゼロが非常に多いため、ゼロと非ゼロの連続数を
		ガンマ符号を用いて符号化します(ゼロ・ラン・レングス)。そのため、ビッ
		ト配列中には基本的に

		1 非ゼロの連続数(ラン・レングス)のガンマ符号
		2 非ゼロの値のゴロム・ライス符号 (連続数分 繰り返される)
		3 ゼロの連続数(ラン・レングス)のガンマ符号

		が複数回現れることになります。

		ガンマ符号は以下の形式で、0を保存する必要がないため(長さ0はあり得な
		いので)、表現できる最低の値は1となっています。

		                  MSB ←    → LSB
		(v=1)                            1
		(2<=v<=3)                      x10          x = v-2
		(4<=v<=7)                    xx100         xx = v-4
		(8<=v<=15)                 xxx1000        xxx = v-8
		(16<=v<=31)             x xxx10000       xxxx = v-16
		(32<=v<=63)           xxx xx100000      xxxxx = v-32
		(64<=v<=127)        xxxxx x1000000     xxxxxx = v-64
		(128<=v<=255)     xxxxxxx 10000000    xxxxxxx = v-128
		(256<=v<=511)    xxxxxxx1 00000000   xxxxxxxx = v-256
		      :                    :                  :
		      :                    :                  :

		ゴロム・ライス符号は以下の方法で符号化します。まず、符号化しようとす
		る値をeとします。eには1以上の正の整数と-1以下の負の整数が存在します。
		これを0以上の整数に変換します。具体的には以下の式で変換を行います。
		(変換後の値をmとする)

		m = ((e >= 0) ? 2*e : -2*e-1) - 1

		この変換により、下の表のように、負のeが偶数のmに、正のeが奇数のmに変
		換されます。

		 e     m
		-1     0
		 1     1
		-2     2
		 2     3
		-3     4
		 3     5
		 :     :
		 :     :

		この値 m がどれぐらいのビット長で表現できるかを予測します。このビッ
		ト長を k とします。k は 0 以上の整数です。しかし予測がはずれ、m が k
		ビットで表現できない場合があります。その場合は、k ビットで表現でき
		なかった分の m、つまり k>>m 個分のゼロを出力し、最後にその終端記号と
		して 1 を出力します。
		m が kビットで表現できた場合は、ただ単に 1 を出力します。そのあと、
		m を下位ビットから k ビット分出力します。

		たとえば e が 66 で k が 5 の場合は以下のようになります。

		m は m = ((e >= 0) ? 2*e : -2*e-1) - 1 の式により 131 となります。
		予測したビット数 5 で表現できる値の最大値は 31 ですので、131 である
		m は k ビットで表現できないということになります。ですので、m >> k
		である 4 個の 0 を出力します。

		0000

		次に、その4個の 0 の終端記号として 1 を出力します。

		10000

		最後に m を下位ビットから k ビット分出力します。131 を２進数で表現
		すると 10000011 となりますが、これの下位から 5 ビット分を出力します。


        MSB←  →LSB
		00011 1 0000      (計10ビット)
		~~~~~ ~ ~~~~
		  c   b   a

		a   (m >> k) 個の 0
		b   a の 終端記号
		c   m の下位 k ビット分

		これが e=66 k=5 を符号化したビット列となります。

		原理的には k の予測は、今までに出力した e の絶対値の合計とそれまでに
		出力した個数を元に、あらかじめ実際の画像の統計から作成されたテーブル
		を参照することで算出します。動作を C++ 言語風に記述すると以下のよう
		になります。

		n = 3; // カウンタ
		a = 0; // e の絶対値の合計

		while(e を読み込む)
		{
			k = table[a][n]; // テーブルから k を参照
			m = ((e >= 0) ? 2*e : -2*e-1) - 1;
			e を m と k で符号化;
			a += m >> 1; // e の絶対値の合計 (ただし m >>1 で代用)
			if(--n < 0) a >>= 1, n = 3;
		}

		4 回ごとに a が半減され n がリセットされるため、前の値の影響は符号化
		が進むにつれて薄くなり、直近の値から効率的に k を予測することができ
		ます。


	参考 :
		Golomb codes
		http://oku.edu.mie-u.ac.jp/~okumura/compression/golomb/
		三重大学教授 奥村晴彦氏によるゴロム符号の簡単な解説
*/

//---------------------------------------------------------------------------
#ifdef WRITE_VSTXT
FILE *vstxt = fopen("vs.txt", "wt");
#endif
#define GOLOMB_GIVE_UP_BYTES 4
void CompressValuesGolomb(TLG6BitStream &bs, char *buf, int size)
{
	// golomb encoding, -- http://oku.edu.mie-u.ac.jp/~okumura/compression/golomb/

	// run-length golomb method
	bs.PutValue(buf[0]?1:0, 1); // initial value state

	int count;

	int n = TVP_TLG6_GOLOMB_N_COUNT - 1; // 個数のカウンタ
	int a = 0; // 予測誤差の絶対値の和

	count = 0;
	for(int i = 0; i < size; i++)
	{
		if(buf[i])
		{
			// write zero count
			if(count) bs.PutGamma(count);

			// count non-zero values
			count = 0;
			int ii;
			for(ii = i; ii < size; ii++)
			{
				if(buf[ii]) count++; else break;
			}

			// write non-zero count
			bs.PutGamma(count);

			// write non-zero values
			for(; i < ii; i++)
			{
				int e = buf[i];
#ifdef WRITE_VSTXT
				fprintf(vstxt, "%d ", e);
#endif
				int k = TVPTLG6GolombBitLengthTable[a][n];
				int m = ((e >= 0) ? 2*e : -2*e-1) - 1;
				int store_limit = bs.GetBytePos() + GOLOMB_GIVE_UP_BYTES;
				bool put1 = true;
				for(int c = (m >> k); c > 0; c--)
				{
					if(store_limit == bs.GetBytePos())
					{
						bs.PutValue(m >> k, 8);
#ifdef WRITE_VSTXT
						fprintf(vstxt, "[ %d ] ", m>>k);
#endif
						put1 = false;
						break;
					}
					bs.Put1Bit(0);
				}
				if(store_limit == bs.GetBytePos())
				{
					bs.PutValue(m >> k, 8);
#ifdef WRITE_VSTXT
					fprintf(vstxt, "[ %d ] ", m>>k);
#endif
					put1 = false;
				}
				if(put1) bs.Put1Bit(1);
				bs.PutValue(m, k);
				a += (m>>1);
				if (--n < 0) {
					a >>= 1; n = TVP_TLG6_GOLOMB_N_COUNT - 1;
				}
			}

#ifdef WRITE_VSTXT
			fprintf(vstxt, "\n");
#endif

			i = ii - 1;
			count = 0;
		}
		else
		{
			// zero
			count ++;
		}
	}

	if(count) bs.PutGamma(count);
}
//---------------------------------------------------------------------------
class TryCompressGolomb
{
	int TotalBits; // total bit count
	int Count; // running count
	int N;
	int A;
	bool LastNonZero;

public:
	TryCompressGolomb()
	{
		Reset();
	}

	TryCompressGolomb(const TryCompressGolomb &ref)
	{
		Copy(ref);
	}

	~TryCompressGolomb() { ; }

	void Copy(const TryCompressGolomb &ref)
	{
		TotalBits = ref.TotalBits;
		Count = ref.Count;
		N = ref.N;
		A = ref.A;
		LastNonZero = ref.LastNonZero;
	}

	void Reset()
	{
		TotalBits = 1;
		Count = 0;
		N = TVP_TLG6_GOLOMB_N_COUNT - 1;
		A = 0;
		LastNonZero = false;
	}

	int Try(char *buf, int size)
	{
		for(int i = 0; i < size; i++)
		{
			if(buf[i])
			{
				// write zero count
				if(!LastNonZero)
				{
					if(Count)
						TotalBits +=
							TLG6BitStream::GetGammaBitLength(Count);

					// count non-zero values
					Count = 0;
				}

				// write non-zero values
				for(; i < size; i++)
				{
					int e = buf[i];
					if(!e) break;
					Count ++;
					int k = TVPTLG6GolombBitLengthTable[A][N];
					int m = ((e >= 0) ? 2*e : -2*e-1) - 1;
					int unexp_bits = (m>>k);
					if(unexp_bits >= (GOLOMB_GIVE_UP_BYTES*8-8/2))
						unexp_bits = (GOLOMB_GIVE_UP_BYTES*8-8/2)+8;
					TotalBits += unexp_bits + 1 + k;
					A += (m>>1);
					if (--N < 0) {
						A >>= 1; N = TVP_TLG6_GOLOMB_N_COUNT - 1;
					}
				}

				// write non-zero count

				i--;
				LastNonZero = true;
			}
			else
			{
				// zero
				if(LastNonZero)
				{
					if(Count)
					{
						TotalBits += TLG6BitStream::GetGammaBitLength(Count);
						Count = 0;
					}
				}

				Count ++;
				LastNonZero = false;
			}
		}
		return TotalBits;
	}

	int Flush()
	{
		if(Count)
		{
			TotalBits += TLG6BitStream::GetGammaBitLength(Count);
			Count = 0;
		}
		return TotalBits;
	}
};
//---------------------------------------------------------------------------
#define DO_FILTER \
	len -= 4; \
	for(d = 0; d < len; d+=4) \
	{ FILTER_FUNC(0); FILTER_FUNC(1); FILTER_FUNC(2); FILTER_FUNC(3); } \
	len += 4; \
	for(; d < len; d++) \
	{ FILTER_FUNC(0); }

void ApplyColorFilter(char * bufb, char * bufg, char * bufr, int len, int code)
{
	int d;
	unsigned char t;
	switch(code)
	{
	case 0:
		break;
	case 1:
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n], \
			bufb[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 2:
		for(d = 0; d < len; d++)
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d];
		break;
	case 3:
		for(d = 0; d < len; d++)
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d];
		break;
	case 4:
		for(d = 0; d < len; d++)
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d];
		break;
	case 5:
		for(d = 0; d < len; d++)
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d];
		break;
	case 6:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufb[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 7:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufb[d+n];
		DO_FILTER
		break;
	case 8:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n];
		DO_FILTER
		break;
	case 9:
		for(d = 0; d < len; d++)
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d];
		break;
	case 10:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n], \
			bufb[d+n] -= bufr[d+n];
		DO_FILTER
		break;
	case 11:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufb[d+n], \
			bufg[d+n] -= bufb[d+n];
		DO_FILTER
		break;
	case 12:
		for(d = 0; d < len; d++)
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d];
		break;
	case 13:
		for(d = 0; d < len; d++)
			bufg[d] -= bufr[d],
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d];
		break;
	case 14:
		for(d = 0; d < len; d++)
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d],
			bufg[d] -= bufr[d];
		break;
	case 15:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufb[d+n]<<1; \
			bufr[d+n] -= t, \
			bufg[d+n] -= t;
		DO_FILTER
		break;
	case 16:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n];
		DO_FILTER
		break;
	case 17:
		for(d = 0; d < len; d++)
			bufr[d] -= bufb[d],
			bufb[d] -= bufg[d];
		break;
	case 18:
		for(d = 0; d < len; d++)
			bufr[d] -= bufb[d];
		break;
	case 19:
		for(d = 0; d < len; d++)
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d];
		break;
	case 20:
		for(d = 0; d < len; d++)
			bufb[d] -= bufr[d];
		break;
	case 21:
		for(d = 0; d < len; d++)
			bufb[d] -= bufg[d]>>1;
		break;
	case 22:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufb[d+n]>>1;
		DO_FILTER
		break;
	case 23:
		for(d = 0; d < len; d++)
			bufg[d] -= bufb[d],
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d];
		break;
	case 24:
		for(d = 0; d < len; d++)
			bufb[d] -= bufr[d],
			bufr[d] -= bufg[d],
			bufg[d] -= bufb[d];
		break;
	case 25:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufg[d+n] -= bufr[d+n]>>1;
		DO_FILTER
		break;
	case 26:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			bufr[d+n] -= bufg[d+n]>>1;
		DO_FILTER
		break;
	case 27:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufr[d+n]>>1; \
			bufg[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 28:
		for(d = 0; d < len; d++)
			bufr[d] -= bufb[d]>>1;
		break;
	case 29:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufg[d+n]>>1; \
			bufr[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 30:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufb[d+n]>>1; \
			bufr[d+n] -= t, \
			bufg[d+n] -= t;
		DO_FILTER
		break;
	case 31:
		for(d = 0; d < len; d++)
			bufb[d] -= bufr[d]>>1;
		break;
	case 32:
		for(d = 0; d < len; d++)
			bufr[d] -= bufb[d]<<1;
		break;
	case 33:
		for(d = 0; d < len; d++)
			bufb[d] -= bufg[d]<<1;
		break;
	case 34:
		#undef FILTER_FUNC
		#define FILTER_FUNC(n) \
			t = bufr[d+n]<<1; \
			bufg[d+n] -= t, \
			bufb[d+n] -= t;
		DO_FILTER
		break;
	case 35:
		for(d = 0; d < len; d++)
			bufg[d] -= bufb[d]<<1;
		break;
	case 36:
		for(d = 0; d < len; d++)
			bufr[d] -= bufg[d]<<1;
		break;
	case 37:
		for(d = 0; d < len; d++)
			bufr[d] -= bufg[d]<<1,
			bufb[d] -= bufg[d]<<1;
		break;
	case 38:
		for(d = 0; d < len; d++)
			bufg[d] -= bufr[d]<<1;
		break;
	case 39:
		for(d = 0; d < len; d++)
			bufb[d] -= bufr[d]<<1;
		break;

	}

}
//---------------------------------------------------------------------------
int DetectColorFilter(unsigned char *b, unsigned char *g, unsigned char *r, int size, int &outsize)
{
#ifndef FILTER_TEST
	int minbits = -1;
	int mincode = -1;

	char bbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	char gbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	char rbuf[H_BLOCK_SIZE*W_BLOCK_SIZE];
	TryCompressGolomb bc, gc, rc;

	for(int code = 0; code < FILTER_TRY_COUNT; code++)   // 17..27 are currently not used
	{
		// copy bbuf, gbuf, rbuf into b, g, r.
		memcpy(bbuf, b, sizeof(char)*size);
		memcpy(gbuf, g, sizeof(char)*size);
		memcpy(rbuf, r, sizeof(char)*size);

		// copy compressor
		bc.Reset();
		gc.Reset();
		rc.Reset();

		// Apply color filter
		ApplyColorFilter(bbuf, gbuf, rbuf, size, code);

		// try to compress
		int bits;
		bits  = (bc.Try(bbuf, size), bc.Flush());
		if(minbits != -1 && minbits < bits) continue;
		bits += (gc.Try(gbuf, size), gc.Flush());
		if(minbits != -1 && minbits < bits) continue;
		bits += (rc.Try(rbuf, size), rc.Flush());

		if(minbits == -1 || minbits > bits)
		{
			minbits = bits, mincode = code;
		}
	}

	outsize = minbits;

	return mincode;
#else
	static int filter = 0;

	int f = filter;

	filter++;
	if(filter >= FILTER_TRY_COUNT) filter = 0;

	outsize = 0;

	return f;
#endif
}
//---------------------------------------------------------------------------

static void TLG6InitializeColorFilterCompressor(SlideCompressor &c)
{
	unsigned char code[4096];
	unsigned char dum[4096];
	unsigned char *p = code;
	for(int i = 0; i < 32; i++)
	{
		for(int j = 0; j < 16; j++)
		{
			p[0] = p[1] = p[2] = p[3] = i;
			p += 4;
			p[0] = p[1] = p[2] = p[3] = j;
			p += 4;
		}
	}

	long dumlen;
	c.Encode(code, 4096, dum, dumlen);
}

//---------------------------------------------------------------------------
// int ftfreq[256] = {0};

/**
 * TLG6画像の保存
 * @param out 出力先
 * @param width 画像横幅
 * @param height 画像縦幅
 * @param colors 色数指定 1/3/4
 * @param callback コールバック用パラメータ
 * @param scanlinecallback 行データを返すコールバック。NULL を返すと中断される。1つ前に渡したバッファは有効である必要がある
 */
int
SaveTLG6(tTJSBinaryStream *out,
		 int width, int height, int colors,
		 void *callbackdata,
		 tTVPGraphicScanLineCallback scanlinecallback)
{
	int ret = TLG_SUCCESS;

#ifdef WRITE_ENTROPY_VALUES
	FILE *vs = fopen("vs.bin", "wb");
#endif

	TVPCreateTable();

	// output stream header
	int n = 0;
	if (!out->WriteBuffer("TLG6.0\x00raw\x1a\x00", 11) ||
		!out->WriteBuffer(&colors, 1) ||
		!out->WriteBuffer(&n, 1) || // data flag (0)
		!out->WriteBuffer(&n, 1) || // color type (0)
		!out->WriteBuffer(&n, 1) || // external golomb table (0)
		!out->WriteInt32(width) ||
		!out->WriteInt32(height)) {
		return TLG_ERROR;
	}

	// compress
	long max_bit_length = 0;

	unsigned char *buf[MAX_COLOR_COMPONENTS];
	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++) buf[i] = NULL;
	char *block_buf[MAX_COLOR_COMPONENTS];
	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++) block_buf[i] = NULL;
	unsigned char *filtertypes = NULL;
	tTJSBinaryStream *memstream = NULL;

	try
	{
		memstream = GetMemoryStream();

		TLG6BitStream bs(memstream);
		
		// allocate buffer
		for(int c = 0; c < colors; c++)
		{
			buf[c] = new unsigned char [W_BLOCK_SIZE * H_BLOCK_SIZE * 3];
			block_buf[c] = new char [H_BLOCK_SIZE * width];
		}
		int w_block_count = (int)((width - 1) / W_BLOCK_SIZE) + 1;
		int h_block_count = (int)((height - 1) / H_BLOCK_SIZE) + 1;
		filtertypes = new unsigned char [w_block_count * h_block_count];

		int fc = 0;
		for(int y = 0; y < height; y += H_BLOCK_SIZE)
		{
			int ylim = y + H_BLOCK_SIZE;
			if(ylim > height) ylim = height;
			int gwp = 0;
			int xp = 0;
			for(int x = 0; x < width; x += W_BLOCK_SIZE, xp++)
			{
				int xlim = x + W_BLOCK_SIZE;
				if(xlim > width) xlim = width;
				int bw = xlim - x;

				int p0size; // size of MED method (p=0)
				int minp = 0; // most efficient method (0:MED, 1:AVG)
				int ft; // filter type
				int wp; // write point
				for(int p = 0; p < 2; p++)
				{
					int dbofs = (p+1) * (H_BLOCK_SIZE * W_BLOCK_SIZE);

					// do med(when p=0) or take average of upper and left pixel(p=1)
					for(int c = 0; c < colors; c++)
					{
						const unsigned char * prev = NULL;
						const unsigned char * current = NULL;

						int wp = 0;
						for(int yy = y; yy < ylim; yy++)
						{
							const unsigned char *scan = (const unsigned char *)scanlinecallback(callbackdata, yy);
							if (scan == NULL) {
								ret = TLG_ABORT;
								goto errend;
							}

							const unsigned char * sl = x*colors + c + scan;

							const unsigned char * usl;
							if(yy >= 1) {
								scan = (const unsigned char *)scanlinecallback(callbackdata, yy-1);
								if (scan == NULL) {
									ret = TLG_ABORT;
									goto errend;
								}
								usl = x*colors + c + scan;
							} else
								usl = NULL;
							for(int xx = x; xx < xlim; xx++)
							{
								unsigned char pa = xx > 0 ? sl[-colors] : 0;
								unsigned char pb = usl ? *usl : 0;
								unsigned char px = *sl;

								unsigned char py;

//								py = 0;
								if(p == 0)
								{
									unsigned char pc = (xx > 0 && usl) ? usl[-colors] : 0;
									unsigned char min_a_b = pa>pb?pb:pa;
									unsigned char max_a_b = pa<pb?pb:pa;

									if(pc >= max_a_b)
										py = min_a_b;
									else if(pc < min_a_b)
										py = max_a_b;
									else
										py = pa + pb - pc;
								}
								else
								{
									py = (pa+pb+1)>>1;
								}
								
								buf[c][wp] = (unsigned char)(px - py);

								wp++;
								sl += colors;
								if(usl) usl += colors;
							}
						}
					}

					// reordering
					// Transfer the data into block_buf (block buffer).
					// Even lines are stored forward (left to right),
					// Odd lines are stored backward (right to left).

					wp = 0;
					for(int yy = y; yy < ylim; yy++)
					{
						int ofs;
						if(!(xp&1))
							ofs = (yy - y)*bw;
						else
							ofs = (ylim - yy - 1) * bw;
						bool dir; // false for forward, true for backward
						if(!((ylim-y)&1))
						{
							// vertical line count per block is even
							dir = ((yy&1) ^ (xp&1)) != 0;
						}
						else
						{
							// otherwise;
							if(xp & 1)
							{
								dir = (yy&1);
							}
							else
							{
								dir = ((yy&1) ^ (xp&1)) != 0;
							}
						}

						if(!dir)
						{
							// forward
							for(int xx = 0; xx < bw; xx++)
							{
								for(int c = 0; c < colors; c++)
									buf[c][wp + dbofs] =
									buf[c][ofs + xx];
								wp++;
							}
						}
						else
						{
							// backward
							for(int xx = bw - 1; xx >= 0; xx--)
							{
								for(int c = 0; c < colors; c++)
									buf[c][wp + dbofs] =
									buf[c][ofs + xx];
								wp++;
							}
						}
					}
				}


				for(int p = 0; p < 2; p++)
				{
					int dbofs = (p+1) * (H_BLOCK_SIZE * W_BLOCK_SIZE);
					// detect color filter
					int size = 0;
					int ft_;
					if(colors >= 3)
						ft_ = DetectColorFilter(
							buf[0] + dbofs,
							buf[1] + dbofs,
							buf[2] + dbofs, wp, size);
					else
						ft_ = 0;

					// select efficient mode of p (MED or average)
					if(p == 0)
					{
						p0size = size;
						ft = ft_;
					}
					else
					{
						if(p0size >= size)
							minp = 1, ft = ft_;
					}
				}

				// Apply most efficient color filter / prediction method
				wp = 0;
				int dbofs = (minp + 1)  * (H_BLOCK_SIZE * W_BLOCK_SIZE);
				for(int yy = y; yy < ylim; yy++)
				{
					for(int xx = 0; xx < bw; xx++)
					{
						for(int c = 0; c < colors; c++)
							block_buf[c][gwp + wp] = buf[c][wp + dbofs];
						wp++;
					}
				}

				ApplyColorFilter(block_buf[0] + gwp,
					block_buf[1] + gwp, block_buf[2] + gwp, wp, ft);

				filtertypes[fc++] = (ft<<1) + minp;
//				ftfreq[ft]++;
				gwp += wp;
			}

			// compress values (entropy coding)
			for(int c = 0; c < colors; c++)
			{
				int method;
				CompressValuesGolomb(bs, block_buf[c], gwp);
				method = 0;
#ifdef WRITE_ENTROPY_VALUES
				fwrite(block_buf[c], 1, gwp, vs);
#endif
				long bitlength = bs.GetBitLength();
				if(bitlength & 0xc0000000) {
					// "SaveTLG6: Too large bit length (given image may be too large)"
					ret = TLG_ERROR;
					goto errend;
				}
				// two most significant bits of bitlength are
				// entropy coding method;
				// 00 means Golomb method,
				// 01 means Gamma method (implemented but not used),
				// 10 means modified LZSS method (not yet implemented),
				// 11 means raw (uncompressed) data (not yet implemented).
				if(max_bit_length < bitlength) max_bit_length = bitlength;
				bitlength |= (method << 30);
				memstream->WriteInt32(bitlength);
				bs.Flush();
			}

		}


		// write max bit length
		if (!out->WriteInt32(max_bit_length)) {
			ret = TLG_ERROR;
			goto errend;
		}

		// output filter types
		{
			SlideCompressor comp;
			TLG6InitializeColorFilterCompressor(comp);
			unsigned char *outbuf = new unsigned char[fc * 2];
			try
			{
				long outlen;
				comp.Encode(filtertypes, fc, outbuf, outlen);
				if (!out->WriteInt32(outlen) ||
					!out->WriteBuffer(outbuf, outlen)) {
					ret = TLG_ERROR;
					goto errend;
				}
			}
			catch(...)
			{
				delete [] outbuf;
				throw;
			}
			delete [] outbuf;
/*
			FILE *f = fopen("ft.txt", "wt");
			int n = 0;
			for(int y = 0; y < h_block_count; y++)
			{
				for(int x = 0; x < w_block_count; x++)
				{
					int t = filtertypes[n++];
					char b;
					if(t & 1) b = 'A'; else b = 'M';
					t >>= 1;
					fprintf(f, "%c%x", b, t);
				}
				fprintf(f, "\n");
			}
			fclose(f);
*/
		}

		// copy memory stream to output stream
		out->CopyFrom(memstream, 0);
	}
	catch(...)
	{
		for(int i = 0; i < MAX_COLOR_COMPONENTS; i++)
		{
			if(buf[i]) delete [] (buf[i]);
			if(block_buf[i]) delete [] (block_buf[i]);
		}
		if(filtertypes) delete [] filtertypes;
		if(memstream) delete memstream;
		throw;
	}
errend:
	for(int i = 0; i < MAX_COLOR_COMPONENTS; i++)
	{
		if(buf[i]) delete [] (buf[i]);
		if(block_buf) delete [] (block_buf[i]);
	}
	if(filtertypes) delete [] filtertypes;
	if(memstream) delete memstream;

#ifdef WRITE_ENTROPY_VALUES
	fclose(vs);
#endif
/*
	for(int i = 0; i < 256; i++)
	{
		if(ftfreq[i])
		{
			printf("%3d : %d\n", i, ftfreq[i]);
		}
	}
*/
	return ret;
}
//---------------------------------------------------------------------------


