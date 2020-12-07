#include <bits/stdc++.h>
using namespace std;
struct chunk {
    size_t len;
    bool used;
};
struct object {
    size_t idx;
    size_t len;
};
size_t allocate_obj(forward_list<chunk>& chunks, size_t len, size_t mid_space, bool disallow_insufficient_space) {
    len += mid_space;
    size_t offset = 0;
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (!it->used && it->len >= len) {
            assert(it->len != len + mid_space); // safety net in case implementations differ here
            if (disallow_insufficient_space) assert(it->len == len || it->len > len + mid_space);
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
int main(int argc, char** argv) {
    if (argc < 3) {
        printf("%s first_space subsequent_space [print_val=-1] [disallow_insufficient_space]\n", argv[0]);
        return EXIT_FAILURE;
    }
    size_t front_space, mid_space;
    sscanf(argv[1], "%zu", &front_space);
    sscanf(argv[2], "%zu", &mid_space);
    size_t key = -1;
    if (argc > 3) sscanf(argv[3], "%zu", &key);
    bool disallow_insufficient_space = (argc > 4 && argv[4][0] == '1');
    assert(front_space % sizeof(size_t) == 0);
    assert(mid_space % sizeof(size_t) == 0);
    front_space -= mid_space;
    size_t P, S, B;
    scanf("%zu%zu%zu", &P, &S, &B);
    assert(S % sizeof(size_t) == 0);
    assert(S > front_space + mid_space);
    S = (S - front_space - 256) / sizeof(size_t); // additional 256 bytes for ex4 allowance, so it hopefully won't affect ex2
    unique_ptr<size_t[]> shared_data = make_unique<size_t[]>(S);
    forward_list<chunk> chunks;
    chunks.push_front({S, false});
    unique_ptr<object[]> objects = make_unique<object[]>(B);
    size_t next_val = 0;
    fill_n(objects.get(), B, object{-1u, -1u});
    int type;
    while (scanf("%d", &type) != EOF) {
        if (key--==0) {
            printf("Current state:\n");
            size_t i = front_space;
            for (const auto& c : chunks) {
                printf("[ %zu --- %zu ] %s", i, i + c.len * sizeof(size_t), c.used ? "used" : "free");
                if (c.used) {
                    size_t j;
                    for (j=0; j!=B; ++j) {
                        if (objects[j].idx * sizeof(size_t) + front_space == i + mid_space) {
                            printf(" obj:%zu\n", j);
                            break;
                        }
                    }
                    assert(j!=B);
                }
                else{
                    printf("\n");
                }
                i += c.len * sizeof(size_t);
            }
        }
        size_t p, s, b;
        switch (type) {
            case 0: {
                scanf("%zu", &p);
                assert(p < P);
                printf("#%zu: Connected\n", p);
                break;
            }
            case 1: {
                scanf("%zu", &p);
                assert(p < P);
                printf("#%zu: Disconnected\n", p);
                break;
            }
            case 2: {
                scanf("%zu%zu", &p, &b);
                assert(p < P);
                assert(b < B);
                assert(objects[b].idx <= S);
                assert(objects[b].idx + objects[b].len <= S);
                printf("#%zu: Read:", p);
                for (size_t i = objects[b].idx; i != objects[b].idx + objects[b].len; ++i){
                    printf(" %zu", shared_data[i]);
                }
                printf("\n");
                break;
            }
            case 3: {
                scanf("%zu%zu%zu", &p, &b, &s);
                assert(p < P);
                assert(b < B);
                assert(s > 0);
                assert(s <= S);
                objects[b].idx = allocate_obj(chunks, s, mid_space / sizeof(size_t), disallow_insufficient_space);
                objects[b].len = s;
                printf("#%zu: Allocated at offset %zu:", p, objects[b].idx * sizeof(size_t) + front_space);
                for (size_t i = objects[b].idx; i != objects[b].idx + objects[b].len; ++i){
                    printf(" %zu", (shared_data[i] = next_val++));
                }
                printf("\n");
                break;
            }
            case 4: {
                scanf("%zu%zu", &p, &b);
                assert(p < P);
                assert(b < B);
                assert(objects[b].idx <= S);
                assert(objects[b].idx + objects[b].len <= S);
                printf("#%zu: Freed at offset: %zu\n", p, objects[b].idx * sizeof(size_t) + front_space);
                free_obj(chunks, objects[b].idx, mid_space / sizeof(size_t));
                objects[b] = {-1u, -1u};
                break;
            }
        }
    }
}
