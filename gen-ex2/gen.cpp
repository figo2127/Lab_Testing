#include <bits/stdc++.h>
using namespace std;
constexpr int INST_CONNECT = 0;
constexpr int INST_DISCONNECT = 1;
constexpr int INST_READ = 2;
constexpr int INST_ALLOC = 3;
constexpr int INST_FREE = 4;
constexpr size_t MULTIPLIER_READ = 3;
constexpr size_t MULTIPLIER_ALLOC = 1;
constexpr size_t MULTIPLIER_FREE = 1;
struct chunk {
    size_t len;
    bool used;
};
struct object {
    size_t idx;
    size_t len;
};
struct instruction {
    int type;
    size_t p, b, s;
};
size_t allocate_obj(forward_list<chunk>& chunks, size_t len, size_t mid_space) {
    len += mid_space;
    size_t offset = 0;
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (!it->used && it->len >= len) {
            assert(it->len != len + mid_space); // safety net in case implementations differ here
            if (it->len >= len + mid_space) {
                chunks.insert_after(it, {it->len - len, false});
                it->len = len;
            }
            it->used = true;
            return offset + mid_space;
        }
        offset += it->len;
    }
    assert(false); // no suitable space
}
void free_obj(forward_list<chunk>& chunks, size_t idx, size_t mid_space) {
    idx -= mid_space;
    size_t offset = 0;
    auto prev = chunks.end();
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (offset == idx) {
            it->used = false;
            if (prev != chunks.end() && !prev->used) {
                prev->len += it->len;
                ++it;
                chunks.erase_after(prev);
            }
            else {
                prev = it;
                ++it;
            }
            if (it != chunks.end() && !it->used) {
                prev->len += it->len;
                chunks.erase_after(prev);
            }
            return;
        }
        offset += it->len;
        prev = it;
    }
    assert(false); // cannot find object
}
void add_read_instructions(vector<pair<size_t, instruction>>& out, mt19937_64& rng, size_t P, const object* objects, size_t B) {
    for (size_t b=0; b!=B; ++b) {
        const object& obj = objects[b];
        if (obj.idx != -1u) {
            out.emplace_back(MULTIPLIER_READ, instruction{INST_READ, uniform_int_distribution<size_t>(0, P-1)(rng), b});
        }
    }
}
void add_alloc_instructions(vector<pair<size_t, instruction>>& out, mt19937_64& rng, const forward_list<chunk>& chunks, size_t mid_space, size_t P, size_t S, const object* objects, size_t B, bool disallow_insufficient_space) {
    size_t b = 0;
    for (; b!=B; ++b) {
        if (objects[b].idx == -1u) {
            break;
        }
    }
    if (b == B) return;
    size_t max_allowlimit = 0;
    vector<size_t> disallowed_sizes;
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (!it->used) {
            const size_t allowlimit = it->len - mid_space;
            if (max_allowlimit < allowlimit) max_allowlimit = allowlimit;
            if (disallow_insufficient_space) {
                for (size_t i = max(allowlimit, mid_space) - mid_space; i != allowlimit; ++i) {
                    if (disallowed_sizes.empty() || disallowed_sizes.back() < i) {
                        disallowed_sizes.push_back(i);
                    }
                }
            }
            else {
                if (allowlimit > mid_space) {
                    const size_t disallowed_size = allowlimit - mid_space;
                    if (disallowed_sizes.empty() || disallowed_sizes.back() < disallowed_size) {
                        disallowed_sizes.push_back(disallowed_size);
                    }
                }
            }
        }
    }
    if (max_allowlimit == 0) return;
    for (size_t i=0; i!=25; ++i) {
        const size_t len = poisson_distribution<size_t>(16)(rng);
        if (len != 0 && len <= max_allowlimit && !binary_search(disallowed_sizes.begin(), disallowed_sizes.end(), len)) {
            out.emplace_back(MULTIPLIER_ALLOC, instruction{INST_ALLOC, uniform_int_distribution<size_t>(0, P-1)(rng), b, len});
        }
    }
}
void add_free_instructions(vector<pair<size_t, instruction>>& out, mt19937_64& rng, size_t P, const object* objects, size_t B) {
    for (size_t b=0; b!=B; ++b) {
        const object& obj = objects[b];
        if (obj.idx != -1u) {
            out.emplace_back(MULTIPLIER_FREE, instruction{INST_FREE, uniform_int_distribution<size_t>(0, P-1)(rng), b});
        }
    }
}
void apply_inst(const instruction& inst, FILE* test_in, FILE* test_out, forward_list<chunk>& chunks, size_t front_space, size_t mid_space, size_t P, size_t* shared_data, size_t S, object* objects, size_t B, size_t& next_val) {
    fprintf(test_in, "%d ", inst.type);
    switch (inst.type) {
        case INST_READ: {
            fprintf(test_in, "%zu %zu\n", inst.p, inst.b);
            assert(inst.p < P);
            assert(inst.b < B);
            assert(objects[inst.b].idx <= S);
            assert(objects[inst.b].idx + objects[inst.b].len <= S);
            fprintf(test_out, "#%zu: Read:", inst.p);
            for (size_t i = objects[inst.b].idx; i != objects[inst.b].idx + objects[inst.b].len; ++i){
                fprintf(test_out, " %zu", shared_data[i]);
            }
            fprintf(test_out, "\n");
            break;
        }
        case INST_ALLOC: {
            fprintf(test_in, "%zu %zu %zu\n", inst.p, inst.b, inst.s);
            assert(inst.p < P);
            assert(inst.b < B);
            assert(inst.s > 0);
            assert(inst.s <= S);
            objects[inst.b].idx = allocate_obj(chunks, inst.s, mid_space);
            objects[inst.b].len = inst.s;
            fprintf(test_out, "#%zu: Allocated at offset %zu:", inst.p, objects[inst.b].idx * sizeof(size_t) + front_space);
            for (size_t i = objects[inst.b].idx; i != objects[inst.b].idx + objects[inst.b].len; ++i){
                fprintf(test_out, " %zu", (shared_data[i] = next_val++));
            }
            fprintf(test_out, "\n");
            break;
        }
        case INST_FREE: {
            fprintf(test_in, "%zu %zu\n", inst.p, inst.b);
            assert(inst.p < P);
            assert(inst.b < B);
            assert(objects[inst.b].idx <= S);
            assert(objects[inst.b].idx + objects[inst.b].len <= S);
            fprintf(test_out, "#%zu: Freed at offset: %zu\n", inst.p, objects[inst.b].idx * sizeof(size_t) + front_space);
            free_obj(chunks, objects[inst.b].idx, mid_space);
            objects[inst.b] = {-1u, -1u};
            break;
        }
        default: {
            assert(false);
        }
    }
}
int main(int argc, char** argv) {
    if (argc < 7) {
        printf("%s first_space subsequent_space num_instructions seed test.in test.out [disallow_insufficient_space]\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t front_space, mid_space;
    size_t num_insts;
    size_t seed;
    sscanf(argv[1], "%zu", &front_space);
    sscanf(argv[2], "%zu", &mid_space);
    sscanf(argv[3], "%zu", &num_insts);
    sscanf(argv[4], "%zu", &seed);
    bool disallow_insufficient_space = (argc > 7 && argv[7][0] == '1');
    assert(front_space % sizeof(size_t) == 0);
    assert(mid_space % sizeof(size_t) == 0);
    front_space -= mid_space;
    mt19937_64 rng(seed);
    size_t P = uniform_int_distribution<size_t>(2, 10)(rng);
    size_t S = uniform_int_distribution<size_t>(1, 64)(rng) * 4096;
    size_t B = uniform_int_distribution<size_t>(50000, 100000)(rng);
    FILE* test_in = fopen(argv[5], "w");
    FILE* test_out = fopen(argv[6], "w");
    fprintf(test_in, "%zu %zu %zu\n", P, S, B);
    assert(S % sizeof(size_t) == 0);
    assert(S > front_space + mid_space);
    S = (S - front_space - 256) / sizeof(size_t); // additional 256 bytes for ex4 allowance, so it hopefully won't affect ex2
    unique_ptr<size_t[]> shared_data = make_unique<size_t[]>(S);
    forward_list<chunk> chunks;
    chunks.push_front({S, false});
    unique_ptr<object[]> objects = make_unique<object[]>(B);
    size_t next_val = 0;
    fill_n(objects.get(), B, object{-1u, -1u});
    // connect all
    for (size_t p=0; p!=P; ++p) {
        fprintf(test_in, "%d %zu\n", INST_CONNECT, p);
        fprintf(test_out, "#%zu: Connected\n", p);
    }
    for (size_t i=0; i!=num_insts; ++i) {
        // find an instruction type
        vector<pair<size_t, instruction>> choices;
        add_read_instructions(choices, rng, P, objects.get(), B);
        add_alloc_instructions(choices, rng, chunks, mid_space / sizeof(size_t), P, S, objects.get(), B, disallow_insufficient_space);
        add_free_instructions(choices, rng, P, objects.get(), B);
        size_t sum = 0;
        for (const auto& choice : choices) {
            sum += choice.first;
        }
        assert(sum != 0);
        const size_t r = uniform_int_distribution<size_t>(0, sum - 1)(rng);
        size_t cum = 0;
        for (const auto& choice : choices) {
            if (r < cum + choice.first) {
                apply_inst(choice.second, test_in, test_out, chunks, front_space, mid_space / sizeof(size_t), P, shared_data.get(), S, objects.get(), B, next_val);
                break;
            }
            cum += choice.first;
        }
    }
    // disconnect all
    for (size_t p=0; p!=P; ++p) {
        fprintf(test_in, "%d %zu\n", INST_DISCONNECT, p);
        fprintf(test_out, "#%zu: Disconnected\n", p);
    }
}
