//********************************************************************************
// Copyright (c) 2007-2010 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************


//
// A tile is a square matrix, used to tile a larger array
//
#define TILESIZE 64
#define MINPIVOT 1E-12

//static tbb::atomic<int> tiles_created;
//static tbb::atomic<int> tiles_deleted;

//FIXME
//Aprox tile sizes since there is no sinchronization
int tiles_created = 0;
int tiles_deleted = 0;

class tile 
{
    double *m_tile; //[TILESIZE][TILESIZE];  
public:   

    tile() : m_tile(new double[TILESIZE*TILESIZE])
    {
        tiles_created++;
    }

    ~tile()
    {
	delete[] m_tile;
        tiles_deleted++;
    }
    
    tile(const tile& t) : m_tile(new double[TILESIZE*TILESIZE])
    {
		tiles_created++;
        memcpy(m_tile, t.m_tile, sizeof(double)*TILESIZE*TILESIZE);
    }
    

    
    tile& operator=(const tile& t)
    {
		if (this != &t)
        {
            memcpy(m_tile, t.m_tile, sizeof(double)*TILESIZE*TILESIZE);
        }
        return *this;
    }
    
    void set(const int i, const int j, double d) { m_tile[i*TILESIZE+j] = d; }
    double get(const int i, const int j) const { return m_tile[i*TILESIZE+j]; }

    void dump( double epsilon = MINPIVOT ) const {
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                std::cout.width(10);
                double t = get(i,j);
                if (fabs(t) < MINPIVOT) t = 0.0;
                std::cout << t << " ";
            }
            std::cout << std::endl;
        }
    }

    int identity_check( double epsilon = MINPIVOT ) const {
        int ecount = 0;
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                double t = get(i,j);
                if ( i == j && ( fabs(t-1.0) < epsilon ) ) continue;
                if ( fabs(t) < epsilon) continue;
                
                std::cout << "(" << i << "," << j << "):" << t << std::endl;
                ecount++;
            }
        }
        return ecount;
    }

    int zero_check( double epsilon = MINPIVOT ) const
    {
        int ecount = 0;
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                double t = get(i,j);
                if ( fabs(t) < epsilon) continue;
                std::cout << "(" << i << "," << j << "):" << t << std::endl;
                ecount++;
            }
        }
        return ecount;
    }
    
    int equal( tile& t ) const
    {
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                if (get(i,j) != t.get(i,j)) return false;
            }
        }
        return true;
    }

    //b = inverse(*this)
    tile inverse() const
    {
        tile b = *this;
		double row[TILESIZE];
		
        for (int n = 0; n < TILESIZE; n++)
        {
            double pivot = b.get(n,n);
            if (fabs(pivot) < MINPIVOT)
            {
                std::cout <<"Pivot too small! Pivot( " << pivot << ")" << std::endl;
                //b.dump();
                exit(0);
            }
            
            double pivot_inverse = 1/pivot;
            
            row[n]= pivot_inverse;
            b.set(n,n,pivot_inverse);
            for (int j = 0; j < TILESIZE; j++) 
            {
                if (j == n) continue;
                row[j] = b.get(n,j) * pivot_inverse;
                b.set(n,j,row[j]);
            }
        
            for (int i = 0; i < TILESIZE; i++)
            {
                if (i == n) continue;
                double tin = b.get(i,n);
                b.set(i,n, -tin*row[n]); 
                for (int j = 0; j < TILESIZE; j++) 
                {
                    if (j == n) continue;
                    b.set(i,j, b.get(i,j) - tin*row[j]);
                }
            }
        }
        return b;
	}

    // c = this * b
    tile multiply( const tile &b ) const
    {
        tile c;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = 0.0;
                for (int k = 0; k < TILESIZE; k++)
                    t += get(i,k) * b.get(k,j);

                c.set(i,j,t);
            }
        }
        return c;
    }

    // c = -(this * b)
    tile multiply_negate( const tile& b ) const
    {
        tile c;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = 0.0;
                for (int k = 0; k < TILESIZE; k++)
                    t -= get(i,k) * b.get(k,j);
                c.set(i,j,t);
            }
        }
        return c;
    }
    
    // d = this - (b * c)
    tile multiply_subtract(const tile& b, const tile& c ) const
    {
        tile d;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t -= b.get(i,k) * c.get(k,j);
                d.set(i,j,t);
            }
        }
        return d;
    }

    // this =  this - (b * c)
    void multiply_subtract_in_place( const tile& b, const tile& c )
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t -= b.get(i,k) * c.get(k,j);
                set(i,j,t);
            }
        }
    }

    // d = this + (b * c)
    tile multiply_add( const tile& b, const tile& c ) const
    {
        tile d;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t += b.get(i,k) * c.get(k,j);
                d.set(i,j,t);
            }
        }
        return d;
    }

    // this = this + (b * c)
    void multiply_add_in_place( const tile& b, const tile& c )
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t += b.get(i,k) * c.get(k,j);
                set(i,j,t);
            }
        }
    }

    // this = 0.0;
    void zero()
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                set(i,j,0.0);
            }
        }
    }
};
