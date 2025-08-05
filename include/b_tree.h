#pragma once

#include "types.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>

using namespace std;

namespace db
{

    // B-Tree Node class - represents one node in our balanced tree index
    // Template allows us to use different key types (int, string, etc.)
    template <typename KeyType, typename ValueType>
    class BTreeNode
    {
    public:
        // B-Tree properties - these control the tree structure
        static constexpr size_t MAX_KEYS = 4;            // Maximum 4 keys per node (Order 5 B-tree)
        static constexpr size_t MIN_KEYS = MAX_KEYS / 2; // Minimum 2 keys per node (keeps balanced)

        // Data stored in each node
        vector<KeyType> keys;                   // The sorted search keys (like indexes in a book)
        vector<ValueType> values;               // Values associated with each key
        vector<unique_ptr<BTreeNode>> children; // Pointers to child nodes (for internal nodes)
        bool is_leaf;                           // Is this a leaf node (bottom level)?

        // Constructor - creates a new B-tree node
        BTreeNode(bool leaf = true) : is_leaf(leaf)
        {
            keys.reserve(MAX_KEYS);         // Pre-allocate space for efficiency
            values.reserve(MAX_KEYS);       // Avoid memory reallocations
            children.reserve(MAX_KEYS + 1); // One more child than keys
        }

        // Helper methods to check node state
        bool isFull() const { return keys.size() >= MAX_KEYS; }    // Is node at capacity?
        bool isMinimal() const { return keys.size() <= MIN_KEYS; } // Does node have minimum keys?
    };

    // B-Tree class - the main balanced tree data structure for indexing
    template <typename KeyType, typename ValueType>
    class BTree
    {
    private:
        unique_ptr<BTreeNode<KeyType, ValueType>> root; // Root node of the tree

        // Helper method to create new nodes
        unique_ptr<BTreeNode<KeyType, ValueType>> createNode(bool leaf = true)
        {
            return make_unique<BTreeNode<KeyType, ValueType>>(leaf);
        }

        // Split a full child node - this maintains the balanced tree property
        void splitChild(BTreeNode<KeyType, ValueType> *parent, size_t child_index)
        {
            auto child = move(parent->children[child_index]); // Get the full child
            auto new_node = createNode(child->is_leaf);       // Create new node for split

            // Find the middle key to promote up to parent
            size_t mid = child->keys.size() / 2;         // Middle position
            KeyType middle_key = child->keys[mid];       // Key to move up
            ValueType middle_value = child->values[mid]; // Value to move up

            // Move keys and values after middle to the new node (right half)
            for (size_t i = mid + 1; i < child->keys.size(); i++)
            {
                new_node->keys.push_back(move(child->keys[i]));
                new_node->values.push_back(move(child->values[i]));
            }

            // If this is an internal node, also move the children
            if (!child->is_leaf)
            {
                for (size_t i = mid + 1; i < child->children.size(); i++)
                {
                    new_node->children.push_back(move(child->children[i]));
                }
            }

            // Remove the moved keys/values from original child (keep left half)
            child->keys.resize(mid); // Keep only left half
            child->values.resize(mid);
            if (!child->is_leaf)
            {
                child->children.resize(mid + 1); // Keep left children
            }

            // Insert the middle key into parent at correct position
            parent->keys.insert(parent->keys.begin() + child_index, move(middle_key));
            parent->values.insert(parent->values.begin() + child_index, move(middle_value));
            parent->children.insert(parent->children.begin() + child_index + 1, move(new_node));
        }

        // Insert key-value pair into a node that is not full
        void insertNonFull(BTreeNode<KeyType, ValueType> *node, const KeyType &key, const ValueType &value)
        {
            int i = static_cast<int>(node->keys.size()) - 1; // Start from rightmost position

            if (node->is_leaf)
            {
                // This is a leaf node - find correct position and insert directly

                // Shift elements right until we find correct insertion point
                while (i >= 0 && key < node->keys[i])
                {
                    i--; // Move left to find position
                }
                i++; // Move to insertion position

                // Insert key and value at correct position
                node->keys.insert(node->keys.begin() + i, key);
                node->values.insert(node->values.begin() + i, value);
            }
            else
            {
                // This is an internal node - find correct child to insert into
                // Find correct child to descend into
                while (i >= 0 && key < node->keys[i])
                {
                    i--; // Move left to find child
                }
                i++; // Move to correct child index

                // If target child is full, split it first
                if (node->children[i]->isFull())
                {
                    splitChild(node, i); // Split the full child
                    if (key > node->keys[i])
                    {        // Check if key goes to right split
                        i++; // Move to right child after split
                    }
                }

                // Recursively insert into the appropriate child
                insertNonFull(node->children[i].get(), key, value);
            }
        }

        // Search for a key in the B-tree recursively
        ValueType *searchRecursive(BTreeNode<KeyType, ValueType> *node, const KeyType &key)
        {
            if (!node)
                return nullptr; // Key not found

            size_t i = 0;
            // Find first key greater than or equal to search key
            while (i < node->keys.size() && key > node->keys[i])
            {
                i++; // Move right through keys
            }

            // Check if we found exact match
            if (i < node->keys.size() && key == node->keys[i])
            {
                return &node->values[i]; // Found it! Return pointer to value
            }

            // If this is a leaf and we didn't find it, key doesn't exist
            if (node->is_leaf)
            {
                return nullptr; // Not found
            }

            // Continue search in appropriate child
            return searchRecursive(node->children[i].get(), key);
        }

        // Debug method to print the tree structure (helpful for testing)
        void printRecursive(BTreeNode<KeyType, ValueType> *node, int depth = 0)
        {
            if (!node)
                return; // Nothing to print

            string indent(depth * 2, ' '); // Indentation for tree levels
            cout << indent << "Node (leaf: " << node->is_leaf << "): ";

            // Print all keys in this node
            for (size_t i = 0; i < node->keys.size(); i++)
            {
                cout << node->keys[i] << " ";
            }
            cout << endl;

            // If this is an internal node, recursively print all children
            if (!node->is_leaf)
            {
                for (auto &child : node->children)
                {
                    printRecursive(child.get(), depth + 1); // Print child with increased depth
                }
            }
        }

    public:
        // Constructor - creates an empty B-tree with just a root node
        BTree() : root(createNode()) {}

        // Main insert method - adds a key-value pair to the tree
        void insert(const KeyType &key, const ValueType &value)
        {
            auto r = root.get(); // Get current root

            // If root is full, we need to split it and create new root
            if (r->isFull())
            {
                auto new_root = createNode(false);        // Create new internal root
                new_root->children.push_back(move(root)); // Old root becomes child
                root = move(new_root);                    // Update root pointer
                splitChild(root.get(), 0);                // Split the old root
            }

            // Insert into the tree (now guaranteed root is not full)
            insertNonFull(root.get(), key, value);
        }

        // Search for a key and return pointer to its value (or nullptr if not found)
        ValueType *search(const KeyType &key)
        {
            return searchRecursive(root.get(), key);
        }

        // Check if a key exists in the tree (convenience method)
        bool contains(const KeyType &key)
        {
            return search(key) != nullptr;
        }

        // Print the entire tree structure (for debugging and visualization)
        void print()
        {
            printRecursive(root.get());
        }

        // Range query - get all values for keys in range [start, end)
        // This is very efficient in B-trees due to sorted nature
        vector<ValueType> rangeQuery(const KeyType &start, const KeyType &end)
        {
            vector<ValueType> result; // Collect results here
            // Implementation for range queries would traverse tree collecting values in range
            // For now, simple implementation - full implementation would be recursive
            return result;
        }
    };

} // namespace db