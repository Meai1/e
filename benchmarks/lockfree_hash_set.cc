// Copyright (c) 2011, Robert Escriva
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of this project nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// C includes
#include <cstdlib>
#include <stdint.h>

// C++
#include <iostream>

// STL
#include <tr1/memory>
#include <vector>

// po6 includes
#include <po6/threads/thread.h>

// e includes
#include <e/convert.h>
#include <e/lockfree_hash_set.h>

uint64_t
id(const uint64_t& x)
{
    return x;
}

static uint64_t ops;
static uint64_t workunit;
static uint64_t done;
static uint64_t modulus;
static uint16_t table_size;

void
usage();

void
worker_thread(e::lockfree_hash_set<uint64_t, id>* hash_set);

int
main(int argc, char* argv[])
{
    if (argc != 6)
    {
        usage();
        return EXIT_FAILURE;
    }

    uint16_t threads;

    try
    {
        threads = e::convert::to_uint16_t(argv[1]);
        ops = e::convert::to_uint64_t(argv[2]);
        workunit = e::convert::to_uint64_t(argv[3]);
        modulus = e::convert::to_uint64_t(argv[4]);
        table_size = e::convert::to_uint16_t(argv[5]);
        done = 0;
    }
    catch (std::domain_error& e)
    {
        usage();
        std::cerr << "All parameters must be numeric in nature.";
        return EXIT_FAILURE;
    }
    catch (std::out_of_range& e)
    {
        usage();
        std::cerr << "All parameters must be suitably small.";
        return EXIT_FAILURE;
    }

    e::lockfree_hash_set<uint64_t, id> hash_set(table_size);

    std::cout << "benchmark: " << threads << " threads will perform "
              << ops << " insert/remove operations on a hash set of size "
              << table_size << " with keys taken modulo " << modulus << "."
              << std::endl;

    std::vector<std::tr1::shared_ptr<po6::threads::thread> > workers;

    for (uint16_t i = 0; i < threads; ++i)
    {
        std::tr1::shared_ptr<po6::threads::thread> t;
        t.reset(new po6::threads::thread(std::tr1::bind(worker_thread, &hash_set)));
        workers.push_back(t);
        t->start();
    }

    for (uint16_t i = 0; i < threads; ++i)
    {
        workers[i]->join();
    }

    return EXIT_SUCCESS;
}

void
usage()
{
    std::cerr << "Usage: benchmark "
              << "<threads> "
              << "<ops> "
              << "<workunit> "
              << "<modulus> "
              << "<table_size>"
              << std::endl;
    exit(EXIT_FAILURE);
}

void
worker_thread(e::lockfree_hash_set<uint64_t, id>* hash_set)
{
    uint64_t work = __sync_fetch_and_add(&done, workunit);
    unsigned short int seeds[3];
    drand48_data buf;

    seeds[0] = getpid();
    seeds[1] = time(NULL);
    seeds[2] = pthread_self();
    seed48_r(seeds, &buf);

    while (work < ops)
    {
        long int key;
        lrand48_r(&buf, &key);
        key = key % modulus;

        while (true)
        {
            if (hash_set->contains(key))
            {
                hash_set->remove(key);
            }
            else
            {
                if (hash_set->insert(key))
                {
                    break;
                }
            }
        }

        work = __sync_fetch_and_add(&done, workunit);
    }
}
