#include <functional>
#include <vector>
#include <utility>
#include <stdexcept>
#include <memory>

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
    static const size_t MIN_NUM_OF_CELLS;
    static const size_t SCALE;

    using pair_ptr = std::shared_ptr<std::pair<const KeyType, ValueType>>;

    class iterator;
    class const_iterator;

    HashMap(): hasher_() {
        table_.resize(HashMap::MIN_NUM_OF_CELLS);
        current_capacity_ = HashMap::MIN_NUM_OF_CELLS;
    }
    
    HashMap(const Hash& hash_function): hasher_(hash_function) {
        table_.resize(HashMap::MIN_NUM_OF_CELLS);
        current_capacity_ = HashMap::MIN_NUM_OF_CELLS;
    }
    
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end) {
        hasher_ = Hash();
        table_.resize(HashMap::MIN_NUM_OF_CELLS);
        current_capacity_ = HashMap::MIN_NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    
    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, const Hash& hash_function) {
        hasher_ = hash_function;
        table_.resize(HashMap::MIN_NUM_OF_CELLS);
        current_capacity_ = HashMap::MIN_NUM_OF_CELLS;
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }
    
    HashMap(std::initializer_list<std::pair<KeyType, ValueType>> initializer_list, Hash hash_function = Hash()) {
        hasher_ = hash_function;
        table_.resize(HashMap::MIN_NUM_OF_CELLS);
        current_capacity_ = HashMap::MIN_NUM_OF_CELLS;
        for (auto it = initializer_list.begin(); it != initializer_list.end(); ++it) {
            insert(*it);
        }
    }
    
    HashMap(const HashMap& other): hasher_(other.hasher_), table_(other.table_),
                                   current_size_(other.current_size_), current_capacity_(other.current_capacity_) {}
    
    HashMap& operator=(const HashMap& other) {
        hasher_ = other.hasher_;
        current_capacity_ = other.current_capacity_;
        table_ = std::vector<std::vector<pair_ptr>>(other.table_);
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
    
    /* Insert an element into the hashtable by its key.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes more than capacity, we do stop-the-world rebuild which takes O(total_size) time. */
    void insert(const std::pair<const KeyType, ValueType> &pair) {
        size_t hash = hasher_(pair.first) % current_capacity_;
        for (const auto &p : table_[hash]) {
            if (p->first == pair.first) {
                return;
            }
        }
        table_[hash].push_back(pair_ptr(new std::pair<const KeyType, ValueType>(pair)));
        current_size_++;
        check_rebuild();
    }

    /* Erase element by key. If key not found, do nothing.
       Complexity is linear from cell size, but we assume size is O(1).
       If size becomes less then (capacity / 4), stop-the-world and rebuild which takes O(total_size) time. */
    void erase(KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (size_t i = 0; i < table_[hash].size(); i++) {
            auto &pair = table_[hash][i];
            if (pair->first == key) {
                /* erasing pair from cell */
                table_[hash].erase(table_[hash].begin() + i);
                current_size_--;
                check_rebuild();
                break;
            }
        }
    }

    /* Return iterator for an element by key. Returns end() if key not found.
       Complexity is linear from cell size, but we assume size is O(1). */
    iterator find(KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (size_t i = 0; i < table_[hash].size(); i++) {
            if (table_[hash][i]->first == key) {
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
            if (table_[hash][i]->first == key) {
                return const_iterator(this, hash, i);
            }
        }
        return end();
    }

    size_t size() const {
        return current_size_;
    }

    bool empty() const {
        return size() == 0;
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

    Hash hash_function() const {
        return hasher_;
    }

    // Returns an iterator which points to first cell.
    iterator begin() {
        return iterator(this, 0, 0);
    }

    // Returns an iterator which points after last cell.
    iterator end() {
        return iterator(this, table_.size(), 0);
    }

    // Returns an iterator which points to first cell.
    const_iterator begin() const {
        return const_iterator(this, 0, 0);
    }

    // Returns an iterator which points after last cell.
    const_iterator end() const {
        return const_iterator(this, table_.size(), 0);
    }

    /* Return a value by key.
       If key not found, creates new element in hashtable with default value. */
    ValueType& operator[](KeyType key) {
        size_t hash = hasher_(key) % current_capacity_;
        for (const auto &pair : table_[hash]) {
            if (pair->first == key) {
                return pair->second;
            }
        }
        insert(std::make_pair(key, ValueType()));
        return (*this)[key];
    }

    /* Return a value by key.
       If key not found, throws std::out_of_range. */
    const ValueType& at(KeyType key) const {
        size_t hash = hasher_(key) % current_capacity_;
        for (const auto &pair : table_[hash]) {
            if (pair->first == key) {
                return pair->second;
            }
        }
        throw std::out_of_range("ooops, your key is not found");
    }

    /* Iterator for the hash map
       Contains pointer to the HashMap object, and two parameters: cell and positon.
       It means that iterator points to value stored in table_[cell][positon].
       If iterator points to end of table_, it has cell = table_.size(). */
    class iterator {
      public:
        iterator() {}
        
        iterator(HashMap *outer, size_t cell = 0, size_t positon = 0):
                          outer(outer), cell(cell), positon(positon) {
            find_valid_cell();
        }
        
        iterator& operator=(const iterator& other) {
            cell = other.cell;
            positon = other.positon;
            outer = other.outer;
            return (*this);
        } 
        
        /* If iterator points to the end of current cell after incrementing,
           iterator moves to next non-empty cell.
           If iterator stays valid, find_valid_cell does nothing. */
        iterator operator++() {
            positon++;
            find_valid_cell();
            return (*this);
        }
        
        iterator operator++(int) {
            iterator result = (*this);
            ++(*this);
            return result;
        }
        
        std::pair<const KeyType, ValueType>& operator*() const {
            return *outer->table_[cell][positon];
        }
        
        std::pair<const KeyType, ValueType>* operator->() const {
            return &*(outer->table_[cell][positon]);
        }
        
        bool operator==(const iterator& other) const {
            return std::tie(cell, positon, outer) == std::tie(other.cell, other.positon, other.outer);
        }
        
        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

      private:
        // Moves iterator to next valid cell (or to end).
        void find_valid_cell() {
            while (cell < outer->table_.size() && positon == outer->table_[cell].size()) {
                positon = 0;
                cell++;
            }
        }

      private:
        // Iterator points to outer->table_[cell][positon]
        HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
    };

    /* Const iterator for the hash map
       Contains pointer to the HashMap object, and two parameters: cell and positon.
       It means that iterator points to value stored in table_[cell][positon].
       If iterator points to end of table_, it has cell = table_.size(). */
    class const_iterator {
      public:
        const_iterator() {}
        
        const_iterator(const HashMap *outer, size_t cell = 0, size_t positon = 0):
                                outer(outer), cell(cell), positon(positon) {
            find_valid_cell();
        }
        
        const_iterator& operator=(const const_iterator& other) {
            cell = other.cell;
            positon = other.positon;
            outer = other.outer;
            return (*this);
        } 
        
        /* If iterator points to the end of current cell after incrementing,
           iterator moves to next non-empty cell.
           If iterator stays valid, find_valid_cell does nothing. */
        const_iterator operator++() {
            positon++;
            find_valid_cell();
            return (*this);
        }
        
        const_iterator operator++(int) {
            const_iterator result = (*this);
            ++(*this);
            return result;
        }
        
        const std::pair<const KeyType, ValueType>& operator*() const {
            return *outer->table_[cell][positon];
        }
        
        const std::pair<const KeyType, ValueType>* operator->() const {
            return &*(outer->table_[cell][positon]);
        }
        
        bool operator==(const const_iterator& other) const {
            return std::tie(cell, positon, outer) == std::tie(other.cell, other.positon, other.outer);
        }
        
        bool operator!=(const const_iterator& other) const {
            return std::tie(cell, positon, outer) != std::tie(other.cell, other.positon, other.outer);
        }

      private:
        // Moves iterator to next valid cell (or to end).
        void find_valid_cell() {
            while (cell < outer->table_.size() && positon == outer->table_[cell].size()) {
                positon = 0;
                cell++;
            }
        }

      private:
        // Iterator points to outer->table_[cell][positon].
        const HashMap *outer = nullptr;
        size_t cell;
        size_t positon;
    };

  private:
    /* Stop the world: making capacity = size * 2, then replace elements to other table.
       Complexity is O(size). */
    void rebuild() {
        current_capacity_ = std::max(HashMap::MIN_NUM_OF_CELLS, size() * 2);
        std::vector<std::vector<pair_ptr>> for_change(current_capacity_);
        for (size_t i = 0; i < table_.size(); ++i) {
            for (auto &ptr : table_[i]) {
                size_t hash = hasher_(ptr->first) % current_capacity_;
                for_change[hash].push_back(ptr);
            }
        }
        swap(table_, for_change);
    }

    /* Checks that size belongs to [capacity / 4; capacity].
       If not, rebuilds the table. */
    void check_rebuild() {
        if (size() * SCALE < current_capacity_) {
            rebuild();
        }
        if (size() > current_capacity_) {
            rebuild();
        }
    }

  private:
    Hash hasher_;
    std::vector<std::vector<pair_ptr>> table_;

    // Size must be in [capacity / 4; capacity].
    // Capacity not less than MIN_NUM_OF_CELLS.
    size_t current_size_= 0;
    size_t current_capacity_ = 0;
};

template<class KeyType, class ValueType, class Hash>
constexpr size_t HashMap<KeyType, ValueType, Hash>::MIN_NUM_OF_CELLS = 10;

template<class KeyType, class ValueType, class Hash>
constexpr size_t HashMap<KeyType, ValueType, Hash>::SCALE = 4;