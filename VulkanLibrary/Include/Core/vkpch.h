#pragma once

// vulkan and spir-v cross library dependency stuff
#include "vulkan/vulkan.hpp"
#include "spirv_cross/spirv_cross.hpp"
#include "spirv_cross/spirv_glsl.hpp"
#include "spirv-tools/optimizer.hpp"

// glm maths library
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/vec1.hpp>
#include <glm/gtc/quaternion.hpp>

// io
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

// multi threading stuff
#include <thread>
#include <atomic>
#include <mutex>
#include <future>
#include <condition_variable>
#include <latch>
#include <barrier>
#include <stop_token>
#include <semaphore>

// data structures
#include <string>
#include <vector>
#include <array>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <span>
#include <ranges>
#include <initializer_list>
#include <deque>
#include <queue>
#include <stack>
#include <variant>
#include <optional>
#include <expected>

// algorithms
#include <algorithm>
#include <functional>
#include <memory>
#include <exception>
