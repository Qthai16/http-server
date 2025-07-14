#include <vector>
#include <tuple>
#include <algorithm>
#include <queue>

using namespace std;

// free time: next.start - pre.end
// schedule pre
//  gap = pre.start - prepre.end
//  pre: [pre.start - gap, pre.end - gap]
// schedule next
//  gap = nextnext.start - next.end
//  next: [next.start + gap, next.end + gap]


class Solution {
public:
    int maxFreeTime(int eventTime, int k, vector<int> &startTime, vector<int> &endTime) {
        // event time: [0, eventTime]
        const auto n = startTime.size();
        vector<pair<int, int>> events;
        for (auto i = 0; i < startTime.size(); i++) {
            events.push_back({startTime[i], endTime[i]});
        }
        sort(events.begin(), events.end());
        // MinHeap<Ele> heap;
        vector<int> gaps(events.size() + 1, 0);
        gaps[0] = events[0].first;
        gaps[gaps.size()-1] = eventTime - events[events.size()-1].second;
        for (auto i = 0; i < events.size(); i++) {
            if (i + 1 >= events.size()) break;
            gaps[i+1] = events[i + 1].second - events[i].first;
        }
        gaps[gaps.size()-1] = eventTime - events[events.size()-1].second;
        gaps[gaps.size()-1] = eventTime - events[events.size()-1].second;
        // constrain window cannot extend bigger
        int l = 0, r = k + 1;
        int sum = 0;
        for (auto i = 0; i < k + 1; i++) {
            sum += gaps[i];
        }
        for (auto i = r; i < gaps.size(); i++) {
            sum = max(sum, sum + gaps[i] - gaps[i - r]);
        }
        return sum;
    }
};
