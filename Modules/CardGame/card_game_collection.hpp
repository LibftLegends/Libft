#ifndef CARD_GAME_COLLECTION_HPP
# define CARD_GAME_COLLECTION_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_SET_ENTRIES = 512U;
static const uint32_t FT_CARD_GAME_MAX_PRODUCTS = 64U;
static const uint32_t FT_CARD_GAME_MAX_PRODUCT_SLOTS = 16U;
static const uint32_t FT_CARD_GAME_MAX_PACK_ENTRIES = 32U;

static const uint32_t CARD_GAME_RARITY_COMMON = 1U << 0U;
static const uint32_t CARD_GAME_RARITY_UNCOMMON = 1U << 1U;
static const uint32_t CARD_GAME_RARITY_RARE = 1U << 2U;
static const uint32_t CARD_GAME_RARITY_MYTHIC = 1U << 3U;
static const uint32_t CARD_GAME_RARITY_LEGENDARY = 1U << 4U;
static const uint32_t CARD_GAME_RARITY_PROMO = 1U << 5U;

struct card_game_set_entry
{
    uint32_t printing_id;
    uint32_t definition_id;
    uint32_t set_id;
    uint32_t collector_number;
    uint32_t rarity_mask;
    uint32_t forced_rarity_mask;
    uint32_t selection_weight;
    uint32_t treatment_id;
    ft_bool supplemental;
    ft_bool deck_legal;
};

struct card_game_product_slot
{
    uint32_t set_id;
    uint32_t rarity_mask;
    uint32_t treatment_id;
    uint32_t card_count;
    uint32_t selection_weight;
    ft_bool allow_duplicates;
};

struct card_game_product_definition
{
    uint32_t product_id;
    uint32_t slot_count;
    card_game_product_slot slots[FT_CARD_GAME_MAX_PRODUCT_SLOTS];
};

struct card_game_pack_result
{
    uint32_t product_id;
    uint32_t entry_count;
    uint32_t printing_ids[FT_CARD_GAME_MAX_PACK_ENTRIES];
};

struct card_game_collection_entry
{
    uint32_t printing_id;
    uint32_t quantity;
};

class card_game_collection_engine
{
    private:
        uint8_t _initialised_state;
        uint32_t _set_entry_count;
        uint32_t _product_count;
        uint32_t _collection_count;
        card_game_set_entry _set_entries[FT_CARD_GAME_MAX_SET_ENTRIES];
        card_game_product_definition _products[FT_CARD_GAME_MAX_PRODUCTS];
        card_game_collection_entry _collection[FT_CARD_GAME_MAX_SET_ENTRIES];

        card_game_collection_engine(const card_game_collection_engine &other) = delete;
        card_game_collection_engine(card_game_collection_engine &&other) = delete;
        card_game_collection_engine &operator=(
            const card_game_collection_engine &other) = delete;
        card_game_collection_engine &operator=(
            card_game_collection_engine &&other) = delete;

        int32_t find_set_entry(uint32_t printing_id,
            uint32_t *index) const noexcept;
        int32_t find_product(uint32_t product_id,
            uint32_t *index) const noexcept;
        int32_t choose_printing(const card_game_product_slot &slot,
            uint64_t *random_state, const uint32_t *excluded_printing_ids,
            uint32_t excluded_count, uint32_t *printing_id) const noexcept;
        int32_t add_collection(uint32_t printing_id) noexcept;

    public:
        card_game_collection_engine() noexcept;
        ~card_game_collection_engine() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t register_set_entry(const card_game_set_entry &entry) noexcept;
        int32_t register_product(const card_game_product_definition &product) noexcept;
        int32_t get_set_entry(uint32_t printing_id,
            card_game_set_entry *entry) const noexcept;
        int32_t get_product(uint32_t product_id,
            card_game_product_definition *product) const noexcept;
        int32_t open_product(uint32_t product_id, uint64_t *random_state,
            card_game_pack_result *result) noexcept;
        int32_t get_collection(uint32_t printing_id,
            uint32_t *quantity) const noexcept;
        uint32_t set_entry_count() const noexcept;
        uint32_t product_count() const noexcept;
};

#endif
