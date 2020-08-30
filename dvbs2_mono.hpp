#include "stdafx.h"

template<class T = size_t>
using dvbs2_t = typename idxs::dvbs2<T>::value_type;
template<class T = size_t>
constexpr T dvbs2_period_v = idxs::dvbs2<T>::period;
template<class T = size_t>
constexpr T dvbs2_len_v = idxs::dvbs2<T>::len;
template<class T = size_t>
constexpr T dvbs2_rows_v = idxs::dvbs2<T>::rows;
template<class T = size_t>
constexpr T dvbs2_cols_v = idxs::dvbs2<T>::cols;
template<class T = size_t>
using dvbs2_idxs_t = typename std::optional<std::pair<dvbs2_t<T>, dvbs2_t<T>>>;

template<class T = size_t>
class dvbs2 : idxs::dvbs2<T>
{
	static constexpr auto m_idxs{ idxs::dvbs2<T>::get() };

	template<class T>
	static constexpr bool check_synch(T& data)noexcept
	{
		return bool{
			data[0] == 0xB8 &&
			data[1] == 0x47 &&
			data[2] == 0x47 &&
			data[3] == 0x47 &&
			data[4] == 0x47 &&
			data[5] == 0x47 &&
			data[6] == 0x47 &&
			data[7] == 0x47 &&
			data[8] == 0xB8
		};
	}
public:
	static constexpr std::optional<std::pair<dvbs2_t<T>, dvbs2_t<T>>>
		find_synch(std::vector<uint8_t>& data, dvbs2_idxs_t<T> synch_idx)noexcept
	{
		if (synch_idx.has_value())
		{
			auto r{ synch_idx.value().first };
			auto idxs_cols{ m_idxs[0] };
			std::array<T, dvbs2_rows_v<T>> col{};
			for (T idx{}; auto && elm : idxs_cols)
			{
				col[idx++] = shl_elm(data[elm[0]], data[elm[1]], synch_idx.value().second);
			}
			if (check_synch(col))
			{
				return synch_idx;
			}
		}
		for (T idx_byte{}; auto && idxs_cols : m_idxs)
		{
			for (T idx_bit{}; idx_bit < 8; ++idx_bit)
			{
				std::array<T, dvbs2_rows_v<T>> col{};
				for (T idx{}; auto && elm : idxs_cols)
				{
					col[idx++] = shl_elm(data[elm[0]], data[elm[1]], idx_bit);
				}
				if (check_synch(col))
				{
					return std::pair(idx_byte, idx_bit);
				}
			}
			++idx_byte;
		}
		return {};
	}
	static constexpr void
		remove_synch(std::vector<uint8_t>& data, std::pair<dvbs2_t<T>, dvbs2_t<T>>& synch)noexcept
	{
		auto col{ m_idxs[synch.first] };

		auto first{ static_cast<uint8_t>(Tables::Fill(8 - synch.second)) };
		auto second{ static_cast<uint8_t>(~first) };
		data[col[0][0]] = (data[col[0][0]] & second) | (data[col[0][1]] & first);
		data[col[1][0]] = (data[col[1][0]] & second) | (data[col[1][1]] & first);
		data[col[2][0]] = (data[col[2][0]] & second) | (data[col[2][1]] & first);
		data[col[3][0]] = (data[col[3][0]] & second) | (data[col[3][1]] & first);
		data[col[4][0]] = (data[col[4][0]] & second) | (data[col[4][1]] & first);
		data[col[5][0]] = (data[col[5][0]] & second) | (data[col[5][1]] & first);
		data[col[6][0]] = (data[col[6][0]] & second) | (data[col[6][1]] & first);
		data[col[7][0]] = (data[col[7][0]] & second) | (data[col[7][1]] & first);
		data[col[8][0]] = (data[col[8][0]] & second) | (data[col[8][1]] & first);

		data.erase(std::next(std::begin(data), col[0][1]));
		data.erase(std::next(std::begin(data), col[1][1] - 1));
		data.erase(std::next(std::begin(data), col[2][1] - 2));
		data.erase(std::next(std::begin(data), col[3][1] - 3));
		data.erase(std::next(std::begin(data), col[4][1] - 4));
		data.erase(std::next(std::begin(data), col[5][1] - 5));
		data.erase(std::next(std::begin(data), col[6][1] - 6));
		data.erase(std::next(std::begin(data), col[7][1] - 7));
		data.erase(std::next(std::begin(data), col[8][1] - 8));
	}
};