#ifndef __SEGMENTS_H_
#define __SEGMENTS_H_

#include <cstdlib>
#include <cstring>
#include <cerrno>

#define DEFAULT_DATA_SIZE	4
#define DEFAULT_SEGMENTS_COUNT	4
#define DEFAULT_SEGREFS_SIZE	DEFAULT_SEGMENTS_COUNT
#define DEFAULT_DATAREFS_SIZE	128
#define DEFAULT_SEGMENT_LEN	255


class segments_t
{
private:
	class segments_atom_t
	{
	private:
		typedef struct _cstring_t  {
			char* text;
			size_t length;
			size_t size;
		} cstring_t;
	public:
		segments_atom_t() : data_size(DEFAULT_DATA_SIZE), data_count(0) {
			size_t i;

			data = (cstring_t*)malloc( sizeof(cstring_t) * data_size );
			for ( i = 0; i < data_size; i++ ) {
				data[i].text = 0x00;
				data[i].size = \
				data[i].length \
					= 0x00;
			}
		}
		~segments_atom_t() {
			size_t i;
			for ( i = 0; i < data_size; i++ ) {
				if ( data[i].size )
					free( data[i].text );
			}
			free( data );
		}
		void push( const size_t _segnum, const char* _data ) {
			if ( data_size < _segnum + 1 ) {
				size_t i, old_data_size(data_size);

				data_size = _segnum * 2;
				data = (cstring_t*)realloc( (void*)data, sizeof(cstring_t) * data_size );

				for ( i = old_data_size; i < data_size; i++ ) {
					data[i].text = 0x00;
					data[i].size = \
					data[i].length \
						= 0x00;
				}
			}

			if ( !data[_segnum].size ) {
				data[_segnum].size = DEFAULT_SEGMENT_LEN;
				data[_segnum].text = (char*)malloc( data[_segnum].size );
			}

			size_t i;
			for ( i = 0; _data[i]; i++ ) {
				if ( data[_segnum].size < i + 1 ) {
					data[_segnum].size = i * 2 + 2;
					data[_segnum].text = (char*)realloc( (void*)data[_segnum].text, data[_segnum].size );
				}

				data[_segnum].text[i] = _data[i];
				data[_segnum].length = i;
			}
			data[_segnum].text[++data[_segnum].length] = 0x00;

			data_count++;
		}
		size_t pop( char* _buffer ) {
			size_t i, ii, retval(0);
			for ( i = 0; i < data_size; i++ ) {
				if ( data[i].length ) {
					for ( ii = 0; ii < data[i].length; ii++ )
						_buffer[retval++] = data[i].text[ii];
					data[i].length = 0;
				}
			}
			_buffer[retval] = 0x00;
			data_count = 0;
			return retval;
		}
		size_t getreslen() {
			size_t i, retval(0);
			for ( i = 0; i < data_size; i++ ) {
				retval += data[i].length;
			}
			return( retval );
		}
		size_t getsegcnt() {
			return data_count;
		}
	private:
		cstring_t* data;
		size_t data_size;
		size_t data_count;
	};
public:
	segments_t() : data_size(DEFAULT_DATA_SIZE) {
		size_t i;

		data = (segments_atom_t**)malloc( sizeof(segments_atom_t*) * data_size );
		for ( i = 0; i < data_size; i++ ) {
			data[i] = NULL;
		}
	}
	~segments_t() {
		size_t i;
		for ( i = 0; i < data_size; i++ ) {
			if ( data[i] )
				delete data[i];
		}
		free(data);
	}

	void push( const size_t _ref, const size_t _seg, const char* _data ) {
		if ( data_size < _ref ) {
			size_t i, old_data_size(data_size);
			data_size = _ref * 2;
			data = (segments_atom_t**)realloc( (void*)data, sizeof(segments_atom_t*) * data_size );
			for ( i = old_data_size; i < data_size; i++ ) {
				data[i] = NULL;
			}
		}
		if ( !data[_ref] ) {
			data[_ref] = new segments_atom_t();
		}
		data[_ref]->push( _seg, _data );
	}
	size_t pop( const size_t _ref, char* _buffer ) {
		return data[_ref]->pop( _buffer );
	}
	size_t getreslen( const size_t _ref ) {
		return data[_ref]->getreslen();
	}
	size_t getsegcnt( const size_t _ref ) {
		return data[_ref]->getsegcnt();
	}

private:
	segments_atom_t** data;
	size_t data_size;
};

#endif
