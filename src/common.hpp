#include <cstdint>
#include <algorithm>
#include <type_traits>

template <std::size_t N> struct type_of_lenght_helper { typedef char type[N]; };
template <typename T, std::size_t Size> typename type_of_lenght_helper<Size>::type& lenghtof_for_static_arrays_helper(T(&)[Size]);
#define LENGTHOF_ARRAY(pArray) sizeof(lenghtof_for_static_arrays_helper(pArray))


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

	bool operator==(cstr const& o) const { return equals(o); }
};

template <>
struct std::hash < cstr > {
	using argument_type = cstr;
	using result_type = std::size_t;
	result_type operator()(argument_type const& s) const;
};

template <typename T, typename TReleaser>
struct base_ptr {
	T* p;
	base_ptr(T* p = nullptr) : p(p) {}
	~base_ptr() { reset(); }

	base_ptr(base_ptr& o) = delete;
	base_ptr& operator=(base_ptr& o) = delete;

	base_ptr(base_ptr&& o) {
		reset(o.p);
		o.p = nullptr;
	}
	base_ptr& operator=(base_ptr&& o) {
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
			TReleaser()(p);
		}
		p = ptr;
	}
};

struct moveable_ptr_releaser {
	template <typename T>
	void operator()(T*) { /*nothing*/ }
};

template <typename T>
using moveable_ptr = base_ptr<T, moveable_ptr_releaser>;

struct com_ptr_releaser {
	template <typename T>
	void operator()(T* p) { 
		p->Release();
	}
};

template <typename T>
using com_ptr = base_ptr<T, com_ptr_releaser>;

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
	using Storage = typename ::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type;
	Storage mData;
public:
	struct sScope : noncopyable {
		GlobalSingleton<T>* pObj;
		sScope(GlobalSingleton<T>& obj) : pObj(&obj) { }
		sScope(sScope&& other) : pObj(other.pObj) { other.pObj = nullptr; }
		~sScope() {
			if (pObj)
			{
				pObj->dtor();
			}
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
