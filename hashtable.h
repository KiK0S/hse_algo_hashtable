#include <functional>
#include <vector>
#include <utility>
#include <stdexcept>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
private:
    Hash hasher;
    const size_t NUM_OF_CELLS = 10;
    std::vector<std::vector<std::pair<const KeyType, ValueType>>> table;
    size_t _size = 0;
    /* size must be in [capacity / 4; capacity] */
    size_t capacity = 0;

    /* stop the world: making capacity = size * 2, then replace elements to other table */
    void rebuild() {
        capacity = std::max(NUM_OF_CELLS, size() * 2);
        std::vector<std::vector<std::pair<const KeyType, ValueType>>> for_change(capacity);
        for (auto pair : (*this)) {
            size_t hash = hasher(pair.first) % capacity;
            for_change[hash].push_back(pair);
        }
        swap(table, for_change);
    }
public:
    HashMap(): hasher() {
        table.resize(NUM_OF_CELLS);
        capacity = NUM_OF_CELLS;
    }
    HashMap(const Hash& hash_function): hasher(hash_function) {
        table.resize(NUM_OF_CELLS);
        capacity = NUM_OF_CELLS;
    }
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end) {
        hasher = Hash();
        table.resize(NUM_OF_CELLS);
        capacity = NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, const Hash& hash_function) {
        hasher = hash_function;
        table.resize(NUM_OF_CELLS);
        capacity = NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> initializer_list, Hash hash_function = Hash()) {
        hasher = hash_function;
        table.resize(NUM_OF_CELLS);
        capacity = NUM_OF_CELLS;
        for (auto it = initializer_list.begin(); it != initializer_list.end(); ++it) {
            insert(*it);
        }
    }
    HashMap(const HashMap& other): hasher(other.hasher), table(other.table), _size(other._size), capacity(other.capacity) {}
    HashMap& operator=(const HashMap& other) {
        hasher = other.hasher;
        capacity = other.capacity;
        table = std::vector<std::vector<std::pair<const KeyType, ValueType>>>(other.table);
        _size = other.size();
        return *this;
    }
    HashMap& operator=(const HashMap&& other) {
        hasher = std::move(other.hasher);
        table = std::move(other.table);
        _size = other.size();
        capacity = other.capacity;
        return *this;
    }
    size_t size() const {
        return _size;
    }
    bool empty() const {
        return size() == 0;
    }
    Hash hash_function() const {
        return hasher;
    }
    void insert(std::pair<const KeyType, ValueType> pair) {
        size_t hash = hasher(pair.first) % capacity;
        for (auto p : table[hash]) {
            if (p.first == pair.first) {
                return;
            }
        }
        table[hash].push_back(pair);
        _size++;
        if (size() > capacity) {
            rebuild();
        }
    }
    void erase(KeyType key) {
        size_t hash = hasher(key) % capacity;
        for (size_t i = 0; i < table[hash].size(); i++) {
            auto pair = table[hash][i];
            if (pair.first == key) {
                /* erasing pair from cell */
                std::vector<std::pair<const KeyType, ValueType>> new_vector;
                for (size_t j = 0; j < i; j++) {
                    auto to_add = table[hash][j];
                    new_vector.push_back(std::make_pair(to_add.first, to_add.second));
                }
                for (size_t j = i + 1; j < table[hash].size(); j++) {
                    auto to_add = table[hash][j];
                    new_vector.push_back(std::make_pair(to_add.first, to_add.second));
                }
                swap(table[hash], new_vector);
                /* erased */
                _size--;
                if (size() * 4 < capacity) {
                    rebuild();
                }
                break;
            }
        }
    }
    class iterator {
    private:
        HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
        /* function to make sure iterator points to something valid (or to end) */
        void fix() {
            while (cell < outer->table.size() && positon == outer->table[cell].size()) {
                positon = 0;
                cell++;
            }
        }
    public:
        iterator() {}
        iterator(HashMap *outer, size_t _cell=0, size_t _positon=0): outer(outer), cell(_cell), positon(_positon) {
            fix();
        }
        iterator& operator=(const iterator& other) {
            cell = other.cell;
            positon = other.positon;
            outer = other.outer;
            return (*this);
        } 
        iterator operator++() {
            positon++;
            fix();
            return (*this);
        }
        iterator operator++(int) {
            iterator result = (*this);
            ++(*this);
            return result;
        }
        std::pair<const KeyType, ValueType>& operator*() const {
            return outer->table[cell][positon];
        }
        std::pair<const KeyType, ValueType>* operator->() const {
            return &(outer->table[cell][positon]);
        }
        bool operator==(const iterator& other) const {
            return std::tie(cell, positon, outer) == std::tie(other.cell, other.positon, other.outer);
        }
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };
    class const_iterator {
    private:
        const HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
        /* function to make sure iterator points to something valid (or to end) */
        void fix() {
            while (cell < outer->table.size() && positon == outer->table[cell].size()) {
                positon = 0;
                cell++;
            }
        }
    public:
        const_iterator() {}
        const_iterator(const HashMap *outer, size_t _cell=0, size_t _positon=0): outer(outer), cell(_cell), positon(_positon) {
            fix();
        }
        const_iterator& operator=(const const_iterator& other) {
            cell = other.cell;
            positon = other.positon;
            outer = other.outer;
            return (*this);
        } 
        const_iterator operator++() {
            positon++;
            fix();
            return (*this);
        }
        const_iterator operator++(int) {
            const_iterator result = (*this);
            ++(*this);
            return result;
        }
        const std::pair<const KeyType, ValueType>& operator*() const {
            return outer->table[cell][positon];
        }
        const std::pair<const KeyType, ValueType>* operator->() const {
            return &(outer->table[cell][positon]);
        }
        bool operator==(const const_iterator& other) const {
            return std::tie(cell, positon, outer) == std::tie(other.cell, other.positon, other.outer);
        }
        bool operator!=(const const_iterator& other) const {
            return std::tie(cell, positon, outer) != std::tie(other.cell, other.positon, other.outer);
        }
    };
    iterator begin() {
        return iterator(this, 0, 0);
    }
    iterator end() {
        return iterator(this, table.size(), 0);
    }
    const_iterator begin() const {
        return const_iterator(this, 0, 0);
    }
    const_iterator end() const {
        return const_iterator(this, table.size(), 0);
    }
    iterator find(KeyType key) {
        size_t hash = hasher(key) % capacity;
        for (size_t i = 0; i < table[hash].size(); i++) {
            if (table[hash][i].first == key) {
                return iterator(this, hash, i);
            }
        }
        return end();
    }
    const_iterator find(KeyType key) const {
        size_t hash = hasher(key) % capacity;
        for (size_t i = 0; i < table[hash].size(); i++) {
            if (table[hash][i].first == key) {
                return const_iterator(this, hash, i);
            }
        }
        return end();    
    }
    ValueType& operator[](KeyType key) {
        size_t hash = hasher(key) % capacity;
        for (auto &pair : table[hash]) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        insert(std::make_pair(key, ValueType()));
        return (*this)[key];
    }
    const ValueType& at(KeyType key) const {
        size_t hash = hasher(key) % capacity;
        for (auto &pair : table[hash]) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        throw std::out_of_range("ooops, your key is not found");
    }
    void clear() {
        for (size_t i = 0; i < capacity; i++) {
            table[i].clear();
        }
        _size = 0;
        rebuild();
    }
};