#ifndef PIPER_ENGINE_STEPS_RANDOM_STEP_H
#define PIPER_ENGINE_STEPS_RANDOM_STEP_H

#include <cstdint>
#include <random>
#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    class Random final : public Step
    {
    public:
        static std::string type_name() { return "random"; }

        void declare_io() override
        {
            declare_output<float>("out", out_);
            auto const seed = static_cast<uint32_t>(std::stoi(member("seed")));
            min_ = std::stof(member("min"));
            max_ = std::stof(member("max"));
            rng_.seed(seed);
        }

        void compute(Stage) override
        {
            std::uniform_real_distribution<float> dist(min_, max_);
            out_ = dist(rng_);
        }

    private:
        float        out_{};
        float        min_{0.0f};
        float        max_{1.0f};
        std::mt19937 rng_;
    };
}

#endif
