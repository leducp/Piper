#ifndef PIPER_APP_ACTIVITY_H
#define PIPER_APP_ACTIVITY_H

namespace piper::studio
{
    // Activity-driven render loop helper. While the budget is non-
    // zero the loop polls events at full rate; once it drains the
    // loop blocks on glfwWaitEvents to spare CPU. Each interaction
    // (mouse move, key press, in-flight ImGui edit) tops the
    // budget back up; redraws then continue for a short tail so
    // animations and hover transitions land before the loop idles.
    class Activity
    {
    public:
        void boost(int frames = 30)
        {
            if (frames > frames_remaining_)
            {
                frames_remaining_ = frames;
            }
        }

        bool active() const { return frames_remaining_ > 0; }

        void tick()
        {
            if (frames_remaining_ > 0)
            {
                --frames_remaining_;
            }
        }

    private:
        int frames_remaining_{0};
    };
}

#endif
