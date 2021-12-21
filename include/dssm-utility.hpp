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
#include <vector>

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
			// コンストラクタ　16でバッファサイズが指定される
			inline RingBuffer()
			{
				setBufferSize(16);
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
			}
			// リングバッファのサイズを指定(0より大の整数、2の冪乗が最適)
			void setBufferSize(unsigned int buffer_size_in)
			{
				if (buffer_size_in > 0)
				{
					// 排他　ミューテックスを取得することで排他処理が行われる
					std::lock_guard<std::recursive_mutex> lock(mtx_);
					// リングバッファのサイズを保持
					buffer_size_ = buffer_size_in;
					// 内部変数の初期化
					buf_tid = -1;
					write_pointer_ = 0;
					// メモリ上にリングバッファを確保 newを使ってるのでmallocの必要なし
					data_.resize(buffer_size_in);
					time_data_.resize(buffer_size_in);
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
				std::lock_guard<std::recursive_mutex> lock(mtx_);
				return buffer_size_;
			}
			//バッファ内の最古のTIDを返す
			int returnLastTid()
			{
				if (buf_tid < 0)
					return buf_tid;
				int tid = buf_tid - buffer_size_ + 1 + 1;
				if (tid < 0)
					return 0;
				return tid;
			}
			//バッファ内最新のTIDを返す
			int returnTopTid()
			{
				return buf_tid;
			}
			// リングバッファにデータを書き込み
			void writeBuffer(T data_in)
			{
				// リングバッファが確保されている場合のみ処理
				if (buffer_size_ > 0)
				{
					// 排他
					std::lock_guard<std::recursive_mutex> lock(mtx_);
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
					fprintf(stderr, "DATA Error: [RingBuffer] Buffer is not allocated.\n");
				}
			}
			void writeBuffer(T data_in, SSM_tid TID_in)
			{
				std::lock_guard<std::recursive_mutex> lock(mtx_);
				write_pointer_ = TID_in;
				write_pointer_ %= buffer_size_;
				buf_tid = TID_in - 1;
				writeBuffer(data_in);
			}
			void writeBuffer(T data_in, SSM_tid TID_in, ssmTimeT time_in)
			{
				std::lock_guard<std::recursive_mutex> lock(mtx_);
				writeTime(TID_in, time_in);
				writeBuffer(data_in, TID_in);
			}
			bool writeTime(SSM_tid TID_in, ssmTimeT time_in)
			{
				// リングバッファが確保されている場合のみ処理
				if (buffer_size_ > 0)
				{
					// 排他
					std::lock_guard<std::recursive_mutex> lock(mtx_);
					// データを書き込み
					time_data_[TID_in % buffer_size_] = time_in;
					return true;
				}
				else
				{
					fprintf(stderr, "TIME Error: [RingBuffer] Buffer is not allocated.\n");
					return false;
				}
			}

			// リングバッファのデータを読み込み(buf_tid: 最新, buf_tid - size + 1 : 最古)
			T readBuffer(int read_pointer_in)
			{
				// 過去にデータが書き込まれていないデータのポインタにはアクセスさせない
				if (read_pointer_in < 0)
					read_pointer_in = buf_tid;
				if (read_pointer_in <= buf_tid)
				{
					// 排他
					std::lock_guard<std::recursive_mutex> lock(mtx_);
					// 現在の最新データのポインタはBUF_TID, 古いほど値が小さくなる(最小: BUF_TID-BUFFER_SIZE)
					uint read_pointer = read_pointer_in % buffer_size_;
					return data_[read_pointer];
				}
				else
				{
					fprintf(stderr, "READ Error1: [RingBuffer] Read pointer is out of buffer. (%d / %d)\n",
							read_pointer_in, buf_tid);
					T data;
					return data;
				}
			}
			T readBuffer(int read_pointer_in, SSM_tid &tid_in, ssmTimeT &time_in)
			{
				// 過去にデータが書き込まれていないデータのポインタにはアクセスさせない
				if (read_pointer_in < 0)
					read_pointer_in = buf_tid;
				if (read_pointer_in <= buf_tid)
				{
					// 排他
					uint read_pointer = read_pointer_in % buffer_size_;
					tid_in = read_pointer;
					time_in = readTime(read_pointer);
					std::lock_guard<std::recursive_mutex> lock(mtx_);
					// 現在の最新データのポインタはBUF_TID, 古いほど値が小さくなる(最小: BUF_TID-BUFFER_SIZE)
					return data_[read_pointer];
				}
				else
				{
					fprintf(stderr, "READ Error2: [RingBuffer] Read pointer is out of buffer. (%d / %d)\n",
							read_pointer_in, buf_tid);
					T data;
					return data;
				}
			}
			ssmTimeT readTime(int read_pointer_in)
			{
				uint read_pointer = read_pointer_in % buffer_size_;
				std::lock_guard<std::recursive_mutex> lock(mtx_);
				return time_data_[read_pointer];
			}

			SSM_tid getTIDfromTime(ssmTimeT time_in)
			{
				SSM_tid tid;
				//SSM_tid *tid_p;
				std::lock_guard<std::recursive_mutex> lock(mtx_);
				SSM_tid top = returnTopTid(), bottom = returnLastTid();
				ssmTimeT top_time = readTime(top);
				ssmTimeT cycle = top_time - readTime(top - 1); //cycleをTOPとその手前の差で計算する
				if (time_in > top_time)
					return top;
				if (time_in < readTime(bottom))
					return SSM_ERROR_PAST;
				tid = top + (SSM_tid)((time_in - top_time) / cycle);
				if (tid > top)
					tid = top;
				else if (tid < bottom)
					tid = bottom;

				while (readTime(tid) < time_in)
					tid++;
				while (readTime(tid) > time_in)
					tid--;

				return tid;
			}

		private:
			// リングバッファのポインタ
			std::vector<T> data_;
			// タイムスタンプのポインタ
			std::vector<ssmTimeT> time_data_;
			// リングバッファのサイズ
			unsigned int buffer_size_ = 0;
			// データの数と位置をカウントするためのTID
			int buf_tid = 0;
			// 書き込みポインタ(最新データのポインタ)
			unsigned int write_pointer_ = 0;
			// mutex(読み書き、バッファサイズ変更を排他)
			std::recursive_mutex mtx_;
		};

	} // namespace rbuffer
} // namespace dssm

#endif // _INC_DSSM_UTILITY_