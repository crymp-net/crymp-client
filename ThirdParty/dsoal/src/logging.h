#ifndef LOGGING_H
#define LOGGING_H

#include <type_traits>

namespace ds {

template<typename T>
constexpr auto to_underlying(T e) noexcept -> std::underlying_type_t<T>
{ return static_cast<std::underlying_type_t<T>>(e); }

} // namespace ds

template<class... Args>
void ignore(Args&&...)
{
}

#define DEBUG(...) ignore(__VA_ARGS__)
#define TRACE(...) ignore(__VA_ARGS__)
#define WARN(...) ignore(__VA_ARGS__)
#define FIXME(...) ignore(__VA_ARGS__)
#define ERR(...) ignore(__VA_ARGS__)

#endif // LOGGING_H
