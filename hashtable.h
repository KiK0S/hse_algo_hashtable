#include <functional>
#include <vector>
#include <utility>
#include <stdexcept>

/* General class for hashtable with closed addressing.
   Basic interface is:
      1. Insert an element by key.
      2. Find an element by key.
      3. Erase an element by key.
   Key must be unique.
   Complexity is amortized O(1) for a query.
   Memory is linear from number of elements inside the table. */
template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
  public:
    // Minimal number of cells. Also used for initialization.
    static const size_t NUM_OF_CELLS;
    static const size_t SCALE;
  private:
    Hash hasher_;
    std::vector<std::vector<std::pair<const KeyType, ValueType>>> table_;

    // Size must be in [capacity / 4; capacity].
    // Capacity not less than NUM_OF_CELLS.
    size_t current_size_= 0;
    size_t current_capacity_ = 0;

    /* Stop the world: making capacity = size * 2, then replace elements to other table.
       Complexity is O(size). */
    void rebuild() {
        current_capacity_ = std::max(HashMap::NUM_OF_CELLS, size() * 2);
        std::vector<std::vector<std::pair<const KeyType, ValueType>>> for_change(current_capacity_);
        for (auto pair : (*this)) {
            size_t hash = hasher_(pair.first) % current_capacity_;
            for_change[hash].push_back(pair);
        }
        swap(table_, for_change);
    }

  public:
    HashMap(): hasher_() {
        table_.resize(HashMap::NUM_OF_CELLS);
        current_capacity_ = HashMap::NUM_OF_CELLS;
    }
    
    HashMap(const Hash& hash_function): hasher_(hash_function) {
        table_.resize(HashMap::NUM_OF_CELLS);
        current_capacity_ = HashMap::NUM_OF_CELLS;
    }
    
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end) {
        hasher_ = Hash();
        table_.resize(HashMap::NUM_OF_CELLS);
        current_capacity_ = HashMap::NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, const Hash& hash_function) {
        hasher_ = hash_function;
        table_.resize(HashMap::NUM_OF_CELLS);
        current_capacity_ = HashMap::NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> initializer_list, Hash hash_function = Hash()) {
        hasher_ = hash_function;
        table_.resize(HashMap::NUM_OF_CELLS);
        current_capacity_ = HashMap::NUM_OF_CELLS;
        for (auto it = initializer_list.begin(); it != initializer_list.end(); ++it) {
            insert(*it);
        }
    }
    
    HashMap(const HashMap& other): hasher_(other.hasher_), table_(other.table_), current_size_(other.current_size_), current_capacity_(other.current_capacity_) {}
    
    HashMap& operator=(const HashMap& other) {
        hasher_ = other.hasher_;
        current_capacity_ = other.current_capacity_;
        table_ = std::vector<std::vector<std::pair<const KeyType, ValueType>>>(other.table_);
        current_size_ = other.size();
        return *this;
    }

    HashMap& operator=(const HashMap&& other) {
        hasher_ = std::move(other.hasher_);
        table_ = std::move(other.table_);
        current_size_ = other.size();
        current_capacity_ = other.current_capacity_;
        return *this;
    }
    
    size_t size() const {
        return current_size_;
    }
    
    bool empty() const {
        return size() == 0;
    }

    Hash hash_function() const {
        return hasher_;
    }

    /* Insert an element into the hashtable by its key.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes more than capacity, we do stop-the-world rebuild which takes O(total_size) time. */
    void insert(std::pair<const KeyType, ValueType> pair) {
        size_t hash = hasher_(pair.first) % current_capacity_;
        for (auto p : table_[hash]) {
            if (p.first == pair.first) {
                return;
            }
        }
        table_[hash].push_back(pair);
        current_size_++;
        if (size() > current_capacity_) {
            rebuild();
        }
    }

    /* Erase element by key. If key not found, do nothing.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes less then (capacity / 4), stop-the-world and rebuild which takes O(total_size) time. */
    void erase(KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (size_t i = 0; i < table_[hash].size(); i++) {
            auto pair = table_[hash][i];
            if (pair.first == key) {
                /* erasing pair from cell */
                std::vector<std::pair<const KeyType, ValueType>> new_vector;
                for (size_t j = 0; j < i; j++) {
                    auto to_add = table_[hash][j];
                    new_vector.push_back(std::make_pair(to_add.first, to_add.second));
                }
                for (size_t j = i + 1; j < table_[hash].size(); j++) {
                    auto to_add = table_[hash][j];
                    new_vector.push_back(std::make_pair(to_add.first, to_add.second));
                }
                swap(table_[hash], new_vector);
                current_size_--;
                if (size() * SCALE < current_capacity_) {
                    rebuild();
                }
                break;
            }
        }
    }

    /* Iterator for the hash map
       Contains pointer to the HashMap object, and two parameters: cell and positon.
       It means that iterator points to value stored in table_[cell][positon].
       If iterator points to end of table_, it has cell = table_.size(). */
    class iterator {
    private:
        // Iterator points to outer->table_[cell][positon]
        HashMap *outer = nullptr;
        size_t cell;
        size_t positon;

        // Function to make sure iterator points to a valid cell (or to end).
        void fix() {
            while (cell < outer->table_.size() && positon == outer->table_[cell].size()) {
                positon = 0;
                cell++;
            }
        }

    public:
        iterator() {}
        
        iterator(HashMap *outer, size_t cell = 0, size_t positon = 0): outer(outer), cell(cell), positon(positon) {
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
            return outer->table_[cell][positon];
        }
        
        std::pair<const KeyType, ValueType>* operator->() const {
            return &(outer->table_[cell][positon]);
        }
        
        bool operator==(const iterator& other) const {
            return std::tie(cell, positon, outer) == std::tie(other.cell, other.positon, other.outer);
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    /* Const iterator for the hash map
       Contains pointer to the HashMap object, and two parameters: cell and positon.
       It means that iterator points to value stored in table_[cell][positon].
       If iterator points to end of table_, it has cell = table_.size(). */
    class const_iterator {
    private:
        // Iterator points to outer->table_[cell][positon].
        const HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
        
        // Function to make sure iterator points to a valid cell (or to end). 
        void fix() {
            while (cell < outer->table_.size() && positon == outer->table_[cell].size()) {
                positon = 0;
                cell++;
            }
        }

    public:
        const_iterator() {}
        
        const_iterator(const HashMap *outer, size_t cell = 0, size_t positon = 0): outer(outer), cell(cell), positon(positon) {
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
            return outer->table_[cell][positon];
        }
        
        const std::pair<const KeyType, ValueType>* operator->() const {
            return &(outer->table_[cell][positon]);
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
        return iterator(this, table_.size(), 0);
    }

    // an iterator which points to first cell
    const_iterator begin() const {
        return const_iterator(this, 0, 0);
    }

    // an iterator which points after last cell
    const_iterator end() const {
        return const_iterator(this, table_.size(), 0);
    }

    /* Return iterator for an element by key. Returns end() if key not found.
       Complexity is linear from cell size, but we assume size is O(1). */
    iterator find(KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (size_t i = 0; i < table_[hash].size(); i++) {
            if (table_[hash][i].first == key) {
                return iterator(this, hash, i);
            }
        }
        return end();
    }

    /* Return const_iterator for an element by key. Returns end() if key not found.
       Complexity is linear from cell size, but we assume size is O(1). */
    const_iterator find(KeyType key) const {
        size_t hash = hasher_(key) % current_capacity_;
        for (size_t i = 0; i < table_[hash].size(); i++) {
            if (table_[hash][i].first == key) {
                return const_iterator(this, hash, i);
            }
        }
        return end();    
    }

    /* Return a value by key.
       If key not found, creates new element in hashtable with default value. */
    ValueType& operator[](KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (auto &pair : table_[hash]) {
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
        size_t hash = hasher_(key) % current_capacity_;
        for (auto &pair : table_[hash]) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        throw std::out_of_range("ooops, your key is not found");
    }

    /* Clear the hashtable
       Complexity is linear from all the elements. */
    void clear() {
        for (size_t i = 0; i < current_capacity_; i++) {
            table_[i].clear();
        }
        current_size_ = 0;
        rebuild();
    }
};

template<class KeyType, class ValueType, class Hash>
constexpr size_t HashMap<KeyType, ValueType, Hash>::NUM_OF_CELLS = 10;

template<class KeyType, class ValueType, class Hash>
constexpr size_t HashMap<KeyType, ValueType, Hash>::SCALE = 4;