/**
 * utility.hpp
 * 2019/12/19 R.Gunji
 * 複数のインスタンスで使用するAPI群
 */

#ifndef _INC_DSSM_UTILITY_
#define _INC_DSSM_UTILITY_

#include <libssm.h>
#include <iostream>
#include <type_traits>
#include <utility>
#include <stdio.h>
#include <mutex>

namespace dssm
{
	namespace util
	{

		/**
		 * 16byteずつhexdumpする。
		 */
		void hexdump(char *p, uint32_t len);

		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * thrd_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countThrdMsgLength();

		/**
		 * 送信時のパケットの大きさを決めるためにインスタンス生成時に呼ばれる。
		 * ssm_msgの構造が変化したらここを変更すること。
		 */
		uint32_t countDssmMsgLength();

	} // namespace util

	namespace rbuffer
	{
		template <typename T>
		class RingBuffer
		{
		public:
			// コンストラクタ　後で必ずバッファサイズ指定すること
			inline RingBuffer()
			{
			}
			// コンストラクタ(リングバッファのサイズを指定(2の冪乗が最適))
			inline RingBuffer(unsigned int buffer_size_in)
			{
				// リングバッファのサイズを指定、メモリ上にバッファを確保
				setBufferSize(buffer_size_in);
			}
			// デストラクタ
			inline ~RingBuffer()
			{
				if (buffer_size_ > 0)
				{
					delete[] data_;
				}
			}
			// リングバッファのサイズを指定(0より大の整数、2の冪乗が最適)
			void setBufferSize(unsigned int buffer_size_in)
			{
				if (buffer_size_in > 0)
				{
					// 排他　ミューテックスを取得することで排他処理が行われる
					std::lock_guard<std::mutex> lock(mtx_);
					// すでにメモリ上にリングバッファが確保されている場合は、一旦削除
					if (buffer_size_ > 0)
					{
						delete data_;
					}
					// リングバッファのサイズを保持
					buffer_size_ = buffer_size_in;
					// 内部変数の初期化
					buf_tid = 0;
					write_pointer_ = 0;
					// メモリ上にリングバッファを確保 newを使ってるのでmallocの必要なし
					data_ = new T[buffer_size_in];
				}
				else
				{
					fprintf(stderr, "Error: [RingBuffer] Please set buffer size more than 0.\n");
				}
			}
			// リングバッファのサイズを取得
			inline unsigned int getBufferSize()
			{
				// 排他
				std::lock_guard<std::mutex> lock(mtx_);
				return buffer_size_;
			}
			// リングバッファにデータを書き込み
			void writeBuffer(T data_in)
			{
				// リングバッファが確保されている場合のみ処理
				if (buffer_size_ > 0)
				{
					// 排他
					std::lock_guard<std::mutex> lock(mtx_);
					// データを書き込み
					data_[write_pointer_] = data_in;
					// 書き込みポインタを進める
					write_pointer_++;
					write_pointer_ %= buffer_size_;
					// 書き込んだデータ数を保持
					buf_tid++;
				}
				else
				{
					fprintf(stderr, "Error: [RingBuffer] Buffer is not allocated.\n");
				}
			}
			void writeBuffer(T data_in, int TID)
			{
				write_pointer_ = TID;
			}
			// リングバッファのデータを読み込み(buf_tid: 最新, buf_tid - size + 1 : 最古)
			T readBuffer(unsigned int read_pointer_in)
			{
				// 過去にデータが書き込まれていないデータのポインタにはアクセスさせない
				if(read_pointer_in < 0) read_pointer_in = buf_tid;
				if (read_pointer_in <= buf_tid)
				{
					// 排他
					std::lock_guard<std::mutex> lock(mtx_);
					// 現在の最新データのポインタはBUF_TID, 古いほど値が小さくなる(最小: BUF_TID-BUFFER_SIZE)
					uint read_pointer = read_pointer_in % buffer_size_;
					return data_[read_pointer];
				}
				else
				{
					fprintf(stderr, "Error: [RingBuffer] Read pointer is out of buffer. (%d / %d)\n",
							read_pointer_in, buf_tid);
					T data;
					return data;
				}
			}

		private:
			// リングバッファのポインタ
			T *data_;
			// リングバッファのサイズ
			unsigned int buffer_size_ = 0;
			// TIDの初期位置
			int buf_tid = -1;
			// 書き込みポインタ(最新データのポインタ)
			unsigned int write_pointer_ = 0;
			// mutex(読み書き、バッファサイズ変更を排他)
			std::mutex mtx_;
		};

	} // namespace rbuffer
} // namespace dssm

#endif // _INC_DSSM_UTILITY_