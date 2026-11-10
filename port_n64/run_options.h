#pragma once
namespace ddsom {
enum class AudioMode { Silent, Recorded, NativeDriver };
struct RunOptions {
 AudioMode audio=AudioMode::Recorded;
 bool accurateVideo=true,videoFastPaths=true,save=true;
};
inline RunOptions g_options;
}
