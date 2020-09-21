#include "stdafx/stdafx.h"

namespace fs = std::filesystem;

template<class T = size_t>
using dvbs2_mono_t = typename idxs::dvbs2_mono<T>::value_type;
template<class T = size_t>
constexpr T dvbs2_mono_period_v = idxs::dvbs2_mono<T>::period;
template<class T = size_t>
constexpr T dvbs2_mono_len_v = idxs::dvbs2_mono<T>::len;
template<class T = size_t>
constexpr T dvbs2_mono_rows_v = idxs::dvbs2_mono<T>::rows;
template<class T = size_t>
constexpr T dvbs2_mono_cols_v = idxs::dvbs2_mono<T>::cols;
template<class T = size_t>
using dvbs2_mono_idxs_t = typename std::optional<std::pair<dvbs2_mono_t<T>, dvbs2_mono_t<T>>>;
template<class T = size_t>
constexpr T dvbs2_mono_pks_v = dvbs2_mono_cols_v<T> * dvbs2_mono_rows_v<T>;

template<class T = size_t>
class dvbs2_mono : idxs::dvbs2_mono<T>
{
    static constexpr auto m_idxs{ idxs::dvbs2_mono<T>::get() };
    static inline std::vector<uint8_t> m_data_prev{};

    template<class V>
    static constexpr bool check_synch(V& data)noexcept;
public:
    template<class C>
    static constexpr void
        find_synch(C&& data, dvbs2_mono_idxs_t<T>& synch_idx)noexcept;
	static constexpr void
        remove_synch(std::vector<uint8_t>& data, dvbs2_mono_idxs_t<T>& synch)noexcept;
    static void
        get_data(std::string in, std::string out, bool is_change_bits = false);
    static void
        get_data_dir(std::string in, std::string out, bool is_change_bits = false);
    static void
        clear()noexcept;
    template<class V, V Byte, V Bit, class Gen = std::vector<uint8_t>(V)>
    static std::optional<std::vector<uint8_t>>
        gen_sig(T size, Gen gen);
};
//////////////////////////////////////////////////////////////
template<class T> template<class V>
constexpr bool dvbs2_mono<T>::check_synch(V& data)noexcept
{
    return bool{
        data[0] == 0xB8 &&
        data[1] == 0x47 &&
        data[2] == 0x47 &&
        data[3] == 0x47 &&
        data[4] == 0x47 &&
        data[5] == 0x47 &&
        data[6] == 0x47 &&
        data[7] == 0x47
    };
}

template<class T> template<class C>
constexpr void
    dvbs2_mono<T>::find_synch(C&& data, dvbs2_mono_idxs_t<T>& synch_idx)noexcept
{
    if (synch_idx.has_value())
    {
        auto idxs_cols{ m_idxs[synch_idx.value().first] };
        std::array<T, dvbs2_mono_rows_v<T>> col{};
        for (T idx{}; auto && elm : idxs_cols)
        {
            col[idx++] = shl_elm(data[elm[0]], data[elm[1]], synch_idx.value().second);
        }
        if (check_synch(col))
        {
            return;
        }
    }
    synch_idx.reset();
    for (T idx_byte{}; auto && idxs_cols : m_idxs)
    {
        for (T idx_bit{}; idx_bit < 8; ++idx_bit)
        {
            std::array<T, dvbs2_mono_rows_v<T>> col{};
            for (T idx{}; auto && elm : idxs_cols)
            {
                col[idx++] = shl_elm(data[elm[0]], data[elm[1]], idx_bit);
            }
            if (check_synch(col))
            {
                synch_idx.emplace(idx_byte, idx_bit);
                return;
            }
        }
        ++idx_byte;
    }
    return;
}

template<class T>
constexpr void
    dvbs2_mono<T>::remove_synch(std::vector<uint8_t>& data, dvbs2_mono_idxs_t<T>& synch)noexcept
{
    auto col{ m_idxs[synch.value().first] };

    auto first{ static_cast<uint8_t>(Tables::Fill(8 - synch.value().second)) };
    auto second{ static_cast<uint8_t>(~first) };
    data[col[0][0]] = (data[col[0][0]] & second) | (data[col[0][1]] & first);
    data[col[1][0]] = (data[col[1][0]] & second) | (data[col[1][1]] & first);
    data[col[2][0]] = (data[col[2][0]] & second) | (data[col[2][1]] & first);
    data[col[3][0]] = (data[col[3][0]] & second) | (data[col[3][1]] & first);
    data[col[4][0]] = (data[col[4][0]] & second) | (data[col[4][1]] & first);
    data[col[5][0]] = (data[col[5][0]] & second) | (data[col[5][1]] & first);
    data[col[6][0]] = (data[col[6][0]] & second) | (data[col[6][1]] & first);
    data[col[7][0]] = (data[col[7][0]] & second) | (data[col[7][1]] & first);

    data.erase(std::next(std::begin(data), col[0][1]));
    data.erase(std::next(std::begin(data), col[1][1] - 1));
    data.erase(std::next(std::begin(data), col[2][1] - 2));
    data.erase(std::next(std::begin(data), col[3][1] - 3));
    data.erase(std::next(std::begin(data), col[4][1] - 4));
    data.erase(std::next(std::begin(data), col[5][1] - 5));
    data.erase(std::next(std::begin(data), col[6][1] - 6));
    data.erase(std::next(std::begin(data), col[7][1] - 7));
}

template<class T>
void
dvbs2_mono<T>::get_data(std::string in, std::string out, bool is_change_bits){
    if(std::filesystem::file_size(in) < dvbs2_mono_pks_v<> + 1){
        return;
    }
    std::ifstream src{in, std::ios::binary};
    std::ofstream dst{out, std::ios::binary};
    dvbs2_mono_idxs_t<> idxs{};

    if(m_data_prev.size() != 0){
        for(size_t pos{}; pos < m_data_prev.size(); ++pos){
            std::vector<uint8_t> data(dvbs2_mono_pks_v<> + 1);
            std::move(
                std::next(std::begin(m_data_prev), pos), std::end(m_data_prev),
                std::begin(data));
            src.read(
                reinterpret_cast<char*>(&data[m_data_prev.size() - pos]),
                data.size() - (m_data_prev.size() - pos));
            if(is_change_bits){
                change_bits(data);
            }
            dvbs2_mono<>::find_synch(data, idxs);
            if(idxs.has_value()){
                dvbs2_mono<>::remove_synch(data, idxs);
                src.seekg(-1, std::ios::cur);
                dst.write(reinterpret_cast<char*>(data.data()), data.size());
            }
            else{
                src.seekg(-(data.size() - (m_data_prev.size() - pos)), std::ios::cur);
            }
        }
    }
    while(!src.fail()){
        std::vector<uint8_t> data(dvbs2_mono_pks_v<> + 1);
        src.read(reinterpret_cast<char*>(data.data()), data.size());
        if(is_change_bits){
            change_bits(data);
        }
        auto counts{src.gcount()};
        if(counts == dvbs2_mono_pks_v<> + 1){
            dvbs2_mono<>::find_synch(data, idxs);
            if(idxs.has_value()){
                dvbs2_mono<>::remove_synch(data, idxs);
                src.seekg(-1, std::ios::cur);
                dst.write(reinterpret_cast<char*>(data.data()), data.size() - 1);
            }
            else{
                src.seekg(-dvbs2_mono_pks_v<>, std::ios::cur);
            }
        }
        else{
            m_data_prev.resize(counts);
            std::move(
                std::begin(data), std::next(std::begin(data), counts),
                std::begin(m_data_prev));
        }
    }
}

template<class T>
void
dvbs2_mono<T>::get_data_dir(std::string in, std::string out, bool is_change_bits){
    std::vector<fs::path> files{};
    std::copy_if(
        fs::directory_iterator{in}, fs::directory_iterator{},
        std::back_inserter(files),
        [](fs::path dir){
            return fs::is_regular_file(dir);
        }
    );
    std::sort(std::begin(files), std::end(files));
    std::string idx{"0"};
    for(auto&& file_in : files){
        get_data(file_in, out + "/" + idx, is_change_bits);
        idx = std::to_string(atoi(idx.data()) + 1);
    }
}

template<class T>
void
dvbs2_mono<T>::clear()noexcept{
    m_data_prev.clear();
}

template<class T> template<class V, V Byte, V Bit, class Gen>
std::optional<std::vector<uint8_t>>
dvbs2_mono<T>::gen_sig(T size, Gen gen){
    if(size < dvbs2_mono_period_v<>){
        return {};
    }
    std::vector<uint8_t> vec(gen(size));
    auto first{ static_cast<uint8_t>(Tables::Fill(8 - Bit)) };
    auto second{ static_cast<uint8_t>(~first) };
    for(T pos_inv{Byte}; pos_inv < size; pos_inv += dvbs2_mono_period_v<>){
        vec[pos_inv] = (second & vec[pos_inv]) | (0xb8 >> Bit);
        vec[pos_inv + 1] = (first & vec[pos_inv + 1]) | (0xb8 << (8 - Bit));
        for(T pos{pos_inv + dvbs2_mono_period_v<>}, idx{}; pos < size && idx < 7; pos += dvbs2_mono_period_v<>, ++idx){
            vec[pos] = (second & vec[pos]) | (0x47 >> Bit);
            vec[pos + 1] = (first & vec[pos + 1]) | (0x47 << (8 - Bit));
            pos_inv = pos;
        }
    }

    return vec;
}
