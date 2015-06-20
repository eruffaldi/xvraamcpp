#include <vector>
#include <algorithm>


template <class T>
class Wrapper
{
public:
	class Iter
	{
	public:
		int _pos;
		Q   _it;
		Iter(const Q i, int pos) : _it(i) ,_pos(pos) {}

		bool operator != (const Iter & other) 
		{
			return _pos != other._pos;
		}

		std::pair<int,Q> operator* () const { return std::make_pair(_pos,*_it); }

		std::pair<int,Q> operator* () { return std::make_pair(_pos,*_it); }
	 
	    const Iter& operator++ ()
	    {
	        ++_pos;
	        ++_it;
	        return *this;
	    }
	};

	T _cont;

	Iter begin() { return Iter(_cont,0); }
	Iter end() { return Iter(_cont,_cont.size());}
	cIter begin() const { return cIter(_cont,0); }
	cIter end() const { return cIter(_cont,_cont.size());}
};

template <class T>
auto enumerate(T & x) -> Wrapper<T> 
{
	return Wrapper<T>(x);
}

// initializer list
// something with std::begin/std::end
// something with .begin and .end

int main(int argc, char const *argv[])
{
	std::vector<int> ciao({10,20,30});
	for(auto q: enumerate(ciao))
	{
		std::cout << q.first << " " << q.second << std::endl;
	}
	return 0;
}