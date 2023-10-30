/* Copyright 2017 https://github.com/mandreyel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies
 * or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef MIO_STRING_UTIL_HEADER
#define MIO_STRING_UTIL_HEADER

#include <type_traits>
#include "Containers/StringConv.h"

namespace mio {
namespace detail {

template<
    typename S,
    typename C = typename std::decay<S>::type,
    typename = decltype(std::declval<C>().data()),
    typename = typename std::enable_if<
        std::is_same<typename C::value_type, char>::value
        || std::is_same<typename C::value_type, TCHAR>::value
    >::type
> struct char_type_helper {
    using type = typename C::value_type;
};

template<class T>
struct char_type {
    using type = typename char_type_helper<T>::type;
};

template<>
struct char_type<char*> {
    using type = char;
};

template<>
struct char_type<const char*> {
    using type = char;
};

template<size_t N>
struct char_type<char[N]> {
    using type = char;
};

template<size_t N>
struct char_type<const char[N]> {
    using type = char;
};

template<>
struct char_type<TCHAR*> {
    using type = TCHAR;
};

template<>
struct char_type<const TCHAR*> {
    using type = TCHAR;
};

template<size_t N>
struct char_type<TCHAR[N]> {
    using type = TCHAR;
};

template<size_t N>
struct char_type<const TCHAR[N]> {
    using type = TCHAR;
};

template<typename CharT, typename S>
struct is_c_str_helper
{
    static constexpr bool value = std::is_same<
        CharT*,
        // TODO: I'm so sorry for this... Can this be made cleaner?
        typename std::add_pointer<
            typename std::remove_cv<
                typename std::remove_pointer<
                    typename std::decay<
                        S
                    >::type
                >::type
            >::type
        >::type
    >::value;
};

template<typename S>
struct is_c_str
{
    static constexpr bool value = is_c_str_helper<char, S>::value;
};

template<typename S>
struct is_c_wstr
{
    static constexpr bool value = is_c_str_helper<TCHAR, S>::value;
};

template<typename S>
struct is_c_str_or_c_wstr
{
    static constexpr bool value = is_c_str<S>::value
        || is_c_wstr<S>::value
        ;
};

template<
    typename String,
    typename = decltype(std::declval<String>().data()),
    typename = typename std::enable_if<!is_c_str_or_c_wstr<String>::value>::type
> const typename char_type<String>::type* c_str(const String& path)
{
    return path.data();
}

template<
    typename String,
    typename = decltype(std::declval<String>().empty()),
    typename = typename std::enable_if<!is_c_str_or_c_wstr<String>::value>::type
> bool empty(const String& path)
{
    return path.empty();
}

template<
    typename String,
    typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
> const typename char_type<String>::type* c_str(String path)
{
    return path;
}

template<
    typename String,
    typename = typename std::enable_if<is_c_str_or_c_wstr<String>::value>::type
> bool empty(String path)
{
    return !path || (*path == 0);
}

struct FPlatformStrFromW
{
#ifdef _WIN32
	FPlatformStrFromW(const TCHAR* From)
		: Str(From)
	{
	}
	const TCHAR* Data() const { return Str; }
	const TCHAR* Str;
#else
	FPlatformStrFromW(const TCHAR* From)
		: Conv(From)
	{
	}
	const ANSICHAR* Data() const { return Conv.Get(); }
	FTCHARToUTF8 Conv;
#endif
};

struct FPlatformStrFromA
{
#ifdef _WIN32
	FPlatformStrFromA(const ANSICHAR* From)
		: Conv(From)
	{
	}
	const TCHAR* Data() const { return Conv.Get(); }
	FUTF8ToTCHAR Conv;
#else
	FPlatformStrFromA(const ANSICHAR* From)
		: Str(From)
	{
	}
	const ANSICHAR* Data() const { return Str; }
	const ANSICHAR* Str;
#endif
};
FPlatformStrFromA ToPlatformStrImpl(const ANSICHAR* From) { return {From}; }
FPlatformStrFromW ToPlatformStrImpl(const TCHAR* From) { return {From}; }
template<typename String>
auto ToPlatformStr(const String& From)
{
	return ToPlatformStrImpl(c_str(From));
}
} // namespace detail
} // namespace mio

#endif // MIO_STRING_UTIL_HEADER

