
int calculateAngle(int hour, int minutes) {
    if (hour < 0 || hour > 12 || minutes < 0 || minutes > 59) {
        throw std::runtime_error("invalid arguments");
    }
    double hDeg = 30*(hour%12) + 0.5*minutes;
    double mDeg = 6*minutes;
    int ret;
    double diff = std::abs(hDeg - mDeg);
    // return acute angle
    if (diff >= 180) return static_cast<int>(360 - diff);
    return static_cast<int>(diff);
}