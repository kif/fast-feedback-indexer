/*
Copyright 2022 Paul Scherrer Institute

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

------------------------

Author: hans-christian.stadler@psi.ch
*/

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <array>
#include "ffbidx/simple_data.h"
#include "ffbidx/indexer.h"

namespace {

    constexpr struct success_type final {} success;
    constexpr struct failure_type final {} failure;

    template <typename stream>
    [[noreturn]] stream& operator<< (stream& out, [[maybe_unused]] const success_type& data)
    {
        out.flush();
        std::exit((EXIT_SUCCESS));
    }

    template <typename stream>
    [[noreturn]] stream& operator<< (stream& out, [[maybe_unused]] const failure_type& data)
    {
        out.flush();
        std::exit((EXIT_FAILURE));
    }

} // namespace

int main (int argc, char *argv[])
{
    using namespace simple_data;

    try {
        if (argc <= 6)
            throw std::runtime_error("missing arguments <file name> <max number of spots> <max number of output cells> <number of kept candidate vectors> <number of half sphere sample points> <redundant computations?>");

        fast_feedback::config_runtime<float> crt{};         // default runtime config
        {
            std::istringstream iss(argv[5]);
            iss >> crt.num_sample_points;
            if (! iss)
                throw std::runtime_error("unable to parse fifth argument: number of half sphere sample points");
            std::cout << "n_samples=" << crt.num_sample_points << '\n';
        }

        fast_feedback::config_persistent<float> cpers{};    // default persistent config
        {
            {
                std::istringstream iss(argv[2]);
                iss >> cpers.max_spots;
                if (! iss)
                    throw std::runtime_error("unable to parse second argument: max number of spots");
                std::cout << "max_spots=" << cpers.max_spots << '\n';
            }
            {
                std::istringstream iss(argv[3]);
                iss >> cpers.max_output_cells;
                if (! iss)
                    throw std::runtime_error("unable to parse third argument: max number of output cells");
                std::cout << "max_output_cells=" << cpers.max_output_cells << '\n';
            }
            {
                std::istringstream iss(argv[4]);
                iss >> cpers.num_candidate_vectors;
                if (! iss)
                    throw std::runtime_error("unable to parse fourth argument: number of kept candidate vectors");
                std::cout << "n_candidates=" << cpers.num_candidate_vectors << '\n';
            }
            {
                std::istringstream iss(argv[6]);
                iss >> std::boolalpha >> cpers.redundant_computations;
                if (! iss)
                    throw std::runtime_error("unable to parse fifth argument: redundant computations? (true|false)");
                std::cout << "redu_comp=" << cpers.redundant_computations << '\n';
            }
        }

        SimpleData<float, raise> data(argv[1]);         // read simple data file

        std::vector<float> x(data.spots.size() + 3);    // coordinate containers
        std::vector<float> y(data.spots.size() + 3);
        std::vector<float> z(data.spots.size() + 3);
        unsigned i=0;
        for (const auto& coord : data.unit_cell) {      // copy cell coordinates
            x[i] = coord.x;
            y[i] = coord.y;
            z[i] = coord.z;
            std::cout << "input" << i << ": " << x[i] << ", " << y[i] << ", " << z[i] << '\n';
            i++;
        }
        for (const auto& coord : data.spots) {          // copy spot coordinates
            x[i] = coord.x;
            y[i] = coord.y;
            z[i] = coord.z;
            i++;            
        }

        std::vector<float> buf(10u*cpers.max_output_cells); // output coordinate container
        fast_feedback::indexer indexer{cpers};          // indexer object

        fast_feedback::memory_pin pin_x{x};             // pin input coordinate containers
        fast_feedback::memory_pin pin_y{y};
        fast_feedback::memory_pin pin_z{z};
        fast_feedback::memory_pin pin_buf{buf};         // pin output coordinate container
        fast_feedback::memory_pin pin_crt{fast_feedback::memory_pin::on(crt)};  // pin runtime config memory

        fast_feedback::input<float> in{{&x[0], &y[0], &z[0]}, {&x[3], &y[3], &z[3]}, 1u, i-3u, true, true}; // create indexer input object
        fast_feedback::output<float> out{&buf[0], &buf[3u*cpers.max_output_cells],
                                         &buf[6u*cpers.max_output_cells], &buf[9u*cpers.max_output_cells],
                                         cpers.max_output_cells};               // create indexer output object

        indexer.index(in, out, crt);                                            // run indexer

        for (unsigned j=0u; j<out.n_cells; j++) {
            std::cout << j << ":cell_score=" << out.score[j] << '\n';
            unsigned b = 3u * j;
            for (unsigned i=0u; i<3u; i++)
                std::cout << j << ":output" << i << ": " << out.x[b+i] << ", " << out.y[b+i] << ", " << out.z[b+i] << '\n';
        }

    } catch (std::exception& ex) {
        std::cerr << "indexing failed: " << ex.what() << '\n' << failure;
    }

    std::cout << success;
}
