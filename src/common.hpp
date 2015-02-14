#include <cstdint>
#include <algorithm>
#include <type_traits>

template <std::size_t N> struct type_of_size_helper { typedef char type[N]; };
template <typename T, std::size_t Size> typename type_of_size_helper<Size>::type& sizeof_for_static_arrays_helper(T(&)[Size]);
#define SIZEOF_ARRAY(pArray) sizeof(sizeof_for_static_arrays_helper(pArray))


struct noncopyable {
	noncopyable() = default;

	noncopyable(noncopyable const&) = delete;
	noncopyable& operator=(noncopyable const&) = delete;
};

class cstr {
public:
	char const* p;

	cstr() : p(nullptr) {}
	cstr(char const* p) : p(p) {}

	operator char const* () const { return p; }

	size_t length() const {
		if (p)
			return ::strlen(p);
		else
			return 0;
	}

	bool ends_with(cstr suffix) const {
		size_t sl = suffix.length();
		size_t l = length();
		if (l < sl) return false;
		return (0 == ::memcmp(&p[l - sl], suffix.p, sl));
	}

	bool starts_with(cstr prefix) const {
		size_t sl = prefix.length();
		size_t l = length();
		if (l < sl) return false;
		return (0 == ::memcmp(p, prefix.p, sl));
	}

	int cmpr(cstr o) const {
		return ::strcmp(p, o.p);
	}

	bool equals(cstr o) const { return cmpr(o) == 0; }
};

template <typename T>
struct moveable_ptr {
	T* p;
	moveable_ptr(T* p = nullptr) : p(p) {}

	moveable_ptr(moveable_ptr& o) = delete;
	moveable_ptr& operator=(moveable_ptr& o) = delete;

	moveable_ptr(moveable_ptr&& o) : p(o.p) { o.p = nullptr; }
	moveable_ptr& operator=(moveable_ptr&& o) { p = o.p; o.p = nullptr; return *this; }

	T* operator->() const { return p; }
	T& operator*() const { return *p; }

	T** pp() { return &p; }

	operator T*() const { return p; }

	void reset() { p = nullptr; }
	void reset(T* ptr) { p = ptr; }
};

template <typename T>
struct com_ptr {
	T* p;
	com_ptr(T* p = nullptr) : p(p) {}
	~com_ptr() { reset(); }

	com_ptr(com_ptr& o) = delete;
	com_ptr& operator=(com_ptr& o) = delete;

	com_ptr(com_ptr&& o) {
		reset(o.p);
		o.p = nullptr; 
	}
	com_ptr& operator=(com_ptr&& o) {
		reset(o.p);
		o.p = nullptr; 
		return *this; 
	}

	T* operator->() const { return p; }
	T& operator*() const { return *p; }

	T** pp() { return &p; }

	operator T*() const { return p; }

	void reset() { reset(nullptr); }
	void reset(T* ptr) { 
		if (p) {
			p->Release();
		}
		p = ptr;
	}
};

template <int A> struct aligned;
template <> struct __declspec(align(1)) aligned < 1 > {};
template <> struct __declspec(align(2)) aligned < 2 > {};
template <> struct __declspec(align(4)) aligned < 4 > {};
template <> struct __declspec(align(8)) aligned < 8 > {};
template <> struct __declspec(align(16)) aligned < 16 > {};
template <> struct __declspec(align(32)) aligned < 32 > {};

template <int size, int align>
union aligned_type {
	aligned<align> mAligned;
	char mPad[size];
};

template <int size, int align>
struct aligned_storage {
	using type = aligned_type < size, align > ;
};


template <typename T>
class GlobalSingleton {
	using Storage = typename aligned_storage<sizeof(T), std::alignment_of<T>::value>::type;
	Storage mData;
public:
	struct sScope : noncopyable {
		GlobalSingleton<T>& obj;
		sScope(GlobalSingleton<T>& obj) : obj(obj) { }
		~sScope() {
			obj.dtor();
		}
	};

	template <typename ... Args>
	void ctor(Args&&... args) {
		::new(&mData) T(std::forward<Args>(args)...);
	}
	template <typename ... Args>
	sScope ctor_scoped(Args&&... args) {
		::new(&mData) T(std::forward<Args>(args)...);
		return sScope(*this);
	}
	void dtor() {
		T& t = get();
		t.~T();
	}
	T& get() { return *reinterpret_cast<T*>(&mData); }
	T const& get() const { return *reinterpret_cast<T const*>(&mData); }

	operator T const& () const { return Get(); }
};


struct sD3DException : public std::exception {
	long hr;
	sD3DException(long hr, char const* const msg) : std::exception(msg), hr(hr) {}
};


void dbg_msg1(cstr format);
void dbg_msg(cstr format, ...);
