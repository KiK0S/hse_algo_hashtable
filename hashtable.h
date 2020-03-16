#include <functional>
#include <vector>
#include <utility>
#include <stdexcept>

template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
  public:
    // Default number of cells.
    static const size_t NUM_OF_CELLS;
  private:
    Hash hasher;
    std::vector<std::vector<std::pair<const KeyType, ValueType>>> table;

    // Size must be in [capacity / 4; max(capacity, NUM_OF_CELLS)].
    size_t current_size = 0;
    size_t current_capacity = 0;

    /* Stop the world: making capacity = size * 2, then replace elements to other table.
       Complexity is O(size). */
    void rebuild() {
        current_capacity = std::max(HashMap::NUM_OF_CELLS, size() * 2);
        std::vector<std::vector<std::pair<const KeyType, ValueType>>> for_change(current_capacity);
        for (auto pair : (*this)) {
            size_t hash = hasher(pair.first) % current_capacity;
            for_change[hash].push_back(pair);
        }
        swap(table, for_change);
    }
  public:
    HashMap(): hasher() {
        table.resize(HashMap::NUM_OF_CELLS);
        current_capacity = HashMap::NUM_OF_CELLS;
    }
    HashMap(const Hash& hash_function): hasher(hash_function) {
        table.resize(HashMap::NUM_OF_CELLS);
        current_capacity = HashMap::NUM_OF_CELLS;
    }
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end) {
        hasher = Hash();
        table.resize(HashMap::NUM_OF_CELLS);
        current_capacity = HashMap::NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, const Hash& hash_function) {
        hasher = hash_function;
        table.resize(HashMap::NUM_OF_CELLS);
        current_capacity = HashMap::NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> initializer_list, Hash hash_function = Hash()) {
        hasher = hash_function;
        table.resize(HashMap::NUM_OF_CELLS);
        current_capacity = HashMap::NUM_OF_CELLS;
        for (auto it = initializer_list.begin(); it != initializer_list.end(); ++it) {
            insert(*it);
        }
    }
    HashMap(const HashMap& other): hasher(other.hasher), table(other.table), current_size(other.current_size), current_capacity(other.current_capacity) {}
    HashMap& operator=(const HashMap& other) {
        hasher = other.hasher;
        current_capacity = other.current_capacity;
        table = std::vector<std::vector<std::pair<const KeyType, ValueType>>>(other.table);
        current_size = other.size();
        return *this;
    }
    HashMap& operator=(const HashMap&& other) {
        hasher = std::move(other.hasher);
        table = std::move(other.table);
        current_size = other.size();
        current_capacity = other.current_capacity;
        return *this;
    }
    
    // Returns number of elements in the table.
    size_t size() const {
        return current_size;
    }
    
    // Returns true if the table is empty.
    bool empty() const {
        return size() == 0;
    }

    // Returns the hasher used in table.
    Hash hash_function() const {
        return hasher;
    }

    /* Insert an element into the hash table by its key.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes more than capacity, we do stop-the-world rebuild which takes O(total_size) time. */
    void insert(std::pair<const KeyType, ValueType> pair) {
        size_t hash = hasher(pair.first) % current_capacity;
        for (auto p : table[hash]) {
            if (p.first == pair.first) {
                return;
            }
        }
        table[hash].push_back(pair);
        current_size++;
        if (size() > current_capacity) {
            rebuild();
        }
    }

    /* Erase element by key. If key not found, do nothing.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes less then (capacity / 4), stop-the-world and rebuild which takes O(total_size) time. */
    void erase(KeyType key) {
        size_t hash = hasher(key) % current_capacity;
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
                current_size--;
                if (size() * 4 < current_capacity) {
                    rebuild();
                }
                break;
            }
        }
    }

    class iterator {
    private:
        // Iterator points to outer->table[cell][positon]
        HashMap *outer = nullptr;
        size_t cell;
        size_t positon;

        // Function to make sure iterator points to a valid cell (or to end).
        void fix() {
            while (cell < outer->table.size() && positon == outer->table[cell].size()) {
                positon = 0;
                cell++;
            }
        }
    public:
        iterator() {}
        iterator(HashMap *outer, size_t cell=0, size_t positon=0): outer(outer), cell(cell), positon(positon) {
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
        // Iterator points to outer->table[cell][positon].
        const HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
        
        // Function to make sure iterator points to a valid cell (or to end). 
        void fix() {
            while (cell < outer->table.size() && positon == outer->table[cell].size()) {
                positon = 0;
                cell++;
            }
        }
    public:
        const_iterator() {}
        const_iterator(const HashMap *outer, size_t cell=0, size_t positon=0): outer(outer), cell(cell), positon(positon) {
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

    // an iterator which points to first cell
    iterator begin() {
        return iterator(this, 0, 0);
    }
    
    // an iterator which points after last cell
    iterator end() {
        return iterator(this, table.size(), 0);
    }

    // an iterator which points to first cell
    const_iterator begin() const {
        return const_iterator(this, 0, 0);
    }

    // an iterator which points after last cell
    const_iterator end() const {
        return const_iterator(this, table.size(), 0);
    }

    /* Return iterator for an element by key. Returns end() if key not found.
       Complexity is linear from cell size, but we assume size is O(1). */
    iterator find(KeyType key) {
        size_t hash = hasher(key) % current_capacity;
        for (size_t i = 0; i < table[hash].size(); i++) {
            if (table[hash][i].first == key) {
                return iterator(this, hash, i);
            }
        }
        return end();
    }

    /* Return const_iterator for an element by key. Returns end() if key not found.
       Complexity is linear from cell size, but we assume size is O(1). */
    const_iterator find(KeyType key) const {
        size_t hash = hasher(key) % current_capacity;
        for (size_t i = 0; i < table[hash].size(); i++) {
            if (table[hash][i].first == key) {
                return const_iterator(this, hash, i);
            }
        }
        return end();    
    }

    /* Return a value by key.
       If key not found, creates new element in hash table with default value. */
    ValueType& operator[](KeyType key) {
        size_t hash = hasher(key) % current_capacity;
        for (auto &pair : table[hash]) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        insert(std::make_pair(key, ValueType()));
        return (*this)[key];
    }

    /* Return a value by key.
       If key not found, throws std::out_of_range. */
    const ValueType& at(KeyType key) const {
        size_t hash = hasher(key) % current_capacity;
        for (auto &pair : table[hash]) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        throw std::out_of_range("ooops, your key is not found");
    }

    /* Clear the hash table
       Complexity is linear from all the elements. */
    void clear() {
        for (size_t i = 0; i < current_capacity; i++) {
            table[i].clear();
        }
        current_size = 0;
        rebuild();
    }
};

template<class KeyType, class ValueType, class Hash>
constexpr size_t HashMap<KeyType, ValueType, Hash>::NUM_OF_CELLS = 10;