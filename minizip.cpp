// ==================== HEADERS & DEFINITIONS ====================
#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <string>
#include <cstdint>

constexpr int BYTE_RANGE = 256;
// A Huffman tree node
struct Node {
    unsigned char data;
    uint32_t freq;
    Node *left, *right;

    Node(unsigned char d, uint32_t f, Node* l = nullptr, Node* r = nullptr)
        : data(d), freq(f), left(l), right(r) {}
};

// Comparator to order the priority_queue as a Min-Heap
struct CompareNode {
    bool operator()(Node* l, Node* r) {
        return l->freq > r->freq; // Lowest frequency has highest priority
    }
};

// Build the Huffman Tree from the 256-byte frequency table
Node* buildHuffmanTree(const std::vector<uint32_t>& freq) {
    std::priority_queue<Node*, std::vector<Node*>, CompareNode> pq;

    for (int i = 0; i < BYTE_RANGE; ++i) {
        if (freq[i] > 0) {
            pq.push(new Node(static_cast<unsigned char>(i), freq[i]));
        }
    }

    // Edge case: file has only 1 unique character
    if (pq.size() == 1) {
        Node* left = pq.top(); pq.pop();
        pq.push(new Node('$', left->freq, left, nullptr));
    }

    while (pq.size() > 1) {
        Node* left = pq.top(); pq.pop();
        Node* right = pq.top(); pq.pop();

        // '$' is a dummy marker for internal nodes
        Node* top = new Node('$', left->freq + right->freq, left, right);
        pq.push(top);
    }

    return pq.empty() ? nullptr : pq.top();
}

// Recursively traverse tree to store bit-strings for each byte
void generateCodes(Node* root, const std::string& str, std::vector<std::string>& codes) {
    if (!root) return;

    // Leaf node reached: save the code string
    if (!root->left && !root->right) {
        codes[root->data] = str;
    }

    generateCodes(root->left, str + "0", codes);
    generateCodes(root->right, str + "1", codes);
}

// Recursively free tree nodes to prevent memory leaks
void freeTree(Node* root) {
    if (!root) return;
    freeTree(root->left);
    freeTree(root->right);
    delete root;
}
class BitWriter {
private:
    std::ofstream& out;
    unsigned char buffer;
    int bitCount;

public:
    BitWriter(std::ofstream& stream) : out(stream), buffer(0), bitCount(0) {}

    // Pack a single bit (0 or 1) into the 8-bit buffer
    void writeBit(int bit) {
        buffer = (buffer << 1) | (bit & 1);
        bitCount++;
        if (bitCount == 8) {
            out.put(static_cast<char>(buffer));
            buffer = 0;
            bitCount = 0;
        }
    }

    // Pad remaining bits with 0s and flush the final byte
    void flush() {
        if (bitCount > 0) {
            buffer <<= (8 - bitCount);
            out.put(static_cast<char>(buffer));
            buffer = 0;
            bitCount = 0;
        }
    }
};
void compressFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open input file.\n";
        std::exit(1);
    }

    std::vector<uint32_t> freq(BYTE_RANGE, 0);
    uint32_t totalBytes = 0;
    char ch;

    // 1. Pass 1: Count byte frequencies
    while (in.get(ch)) {
        freq[static_cast<unsigned char>(ch)]++;
        totalBytes++;
    }

    if (totalBytes == 0) {
        std::cout << "File is empty. Nothing to compress.\n";
        return;
    }

    // 2. Build Huffman Tree & Lookup Table
    Node* root = buildHuffmanTree(freq);
    std::vector<std::string> codes(BYTE_RANGE);
    generateCodes(root, "", codes);

    // 3. Open output file and write Header (Frequency Table + Total Size)
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file.\n";
        freeTree(root);
        std::exit(1);
    }

    out.write(reinterpret_cast<const char*>(freq.data()), BYTE_RANGE * sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(&totalBytes), sizeof(uint32_t));

    // 4. Pass 2: Encode file using BitWriter
    in.clear();                 // Clear EOF flag from Pass 1
    in.seekg(0, std::ios::beg); // Rewind file pointer to start

    BitWriter bw(out);
    while (in.get(ch)) {
        const std::string& code = codes[static_cast<unsigned char>(ch)];
        for (char bitChar : code) {
            bw.writeBit(bitChar - '0');
        }
    }
    bw.flush();

    std::cout << "Compression complete! Original: " << totalBytes 
              << " bytes -> Compressed: " << out.tellp() << " bytes\n";

    freeTree(root);
}

void decompressFile(const std::string& inputPath, const std::string& outputPath) {
    std::ifstream in(inputPath, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open compressed file.\n";
        std::exit(1);
    }

    std::vector<uint32_t> freq(BYTE_RANGE, 0);
    uint32_t totalBytes = 0;

    // 1. Read Header
    in.read(reinterpret_cast<char*>(freq.data()), BYTE_RANGE * sizeof(uint32_t));
    in.read(reinterpret_cast<char*>(&totalBytes), sizeof(uint32_t));

    if (!in) {
        std::cerr << "Error: Invalid or corrupted archive header.\n";
        std::exit(1);
    }

    // 2. Rebuild identical Huffman Tree
    Node* root = buildHuffmanTree(freq);
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file.\n";
        freeTree(root);
        std::exit(1);
    }

    // 3. Decode bits by traversing the tree
    Node* curr = root;
    uint32_t decodedBytes = 0;
    char byteRead;

    while (in.get(byteRead) && decodedBytes < totalBytes) {
        unsigned char uByte = static_cast<unsigned char>(byteRead);
        // Read each bit from Most Significant Bit (7) to Least (0)
        for (int i = 7; i >= 0 && decodedBytes < totalBytes; --i) {
            int bit = (uByte >> i) & 1;
            curr = (bit == 0) ? curr->left : curr->right;

            // Reached leaf node -> write decoded byte and reset to root
            if (!curr->left && !curr->right) {
                out.put(static_cast<char>(curr->data));
                decodedBytes++;
                curr = root;
            }
        }
    }

    std::cout << "Decompression complete! Restored " << decodedBytes << " bytes.\n";
    freeTree(root);
}
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage:\n"
                  << "  Compress:   " << argv[0] << " -c <input_file> <output.huff>\n"
                  << "  Decompress: " << argv[0] << " -d <input.huff> <restored_file>\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "-c") {
        compressFile(argv[2], argv[3]);
    } else if (mode == "-d") {
        decompressFile(argv[2], argv[3]);
    } else {
        std::cerr << "Unknown flag '" << mode << "'. Use -c or -d.\n";
        return 1;
    }

    return 0;
}