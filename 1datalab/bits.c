int bitOr(int x, int y){
	return ~(~x & ~y);
}

int specialBits(){
	return (~0) ^ (0xD7 << 14);
}

int isZero(int x){
	return (!x);
}


int anyEvenBit(int x){
	int ct = 0x55;
	ct += (ct << 8);
	ct += (ct << 16);
	return !(!(x&ct));
}

int rotateLeft(int x, int n){
	int sanshiyijiann = 31 ^ n;
	int fuyi = ~0;
	int A = ((1 << sanshiyijiann) << 1) + fuyi;
	int B = ~A;
	int qbbf = (x & A) << n;
	int hbbf = (x & B) >> sanshiyijiann;
	hbbf = hbbf >> 1;
	hbbf &= ((1<<n)+fuyi);
	return qbbf + hbbf;
}

int bitReverse(int x){
	int const1 = 0xFF + (0xFF<<16);
	int const2 = 0xF + (0xF<<8);
	int const3 = 0x33 + (0x33<<8);
	int const4 = 0x55 + (0x55<<8);
	const2 = const2 + (const2<<16);
	const3 = const3 + (const3<<16);
	const4 = const4 + (const4<<16);
	x = ((x>>16)&(0xFF+(0xFF<<8))) | (x<<16);
	x = ((x>>8)&const1) | ((x<<8)&(~const1));
	x = ((x>>4)&const2) | ((x<<4)&(~const2));
	x = ((x>>2)&const3) | ((x<<2)&(~const3));
	x = ((x>>1)&const4) | ((x<<1)&(~const4));
	return x;
}

/*
int divpwr2(int x, int n){
	int isZ = (!(x>>31)) | (!(x^(1<<31)));
	int mask_Z = (~isZ)+1, mask_F = ~mask_Z;
	return ((x>>n)&mask_Z) + ((~((~x+1)>>n)+1)&mask_F);
}
*/
int divpwr2(int x, int n){
	int fu = !(!(x>>31));
	int neg = (~x)+1;
	int ra_ = !(!(neg&((1<<n)+(~0))));
	int ra = fu & ra_;
	return (x>>n) + (ra);
}

int negate(int x){
	return (~x)+1;
}

int isPower2(int x){
	int Zheng = (!(x>>31)) & (!(!x));
	int Yu = ((~x)+1) & x;
	int Tong = !(Yu^x);
	return Zheng & Tong;
}

int isLess(int x, int y){
	int xZ = !(x>>31), yZ = !(y>>31);
	int FZ = (!xZ) & yZ, TH = !(xZ ^ yZ);
	int Cha = y + ((~x)+1);
	int CZ = (!(Cha>>31)) & (!(!Cha));
	return FZ | (TH&CZ);
}

int leastBitPos(int x){
	int Fan = ~x+1;
	return x & Fan;
}

unsigned float_abs(unsigned uf){
	unsigned exp_ = uf & 0x7F800000;
	unsigned frac_ = uf & 0x007FFFFF;
	if(exp_ == 0x7F800000 && frac_){
		return uf;
	}
	return uf & 0x7FFFFFFF;
}

unsigned float_i2f(int x){
	int zgw = 30, ws = 0;
	int shang = 0, yu = 0, jz = 0, thres = 0;
	int ww = 0;
	int che = 0;
	unsigned ans = 0;
	if(x == 0) return 0;
	if(x == 0x80000000) return 0xCF000000;
	if(x < 0) {
		ans = ans | 0x80000000;
		x = -x;
	}
	while(1){
		che = (1 << zgw);
		if(x & che) break;
		zgw --;
	}
	ww = x - che;
	if(zgw <= 23){
		ws = ww << (23-zgw);
	}
	else{
		jz = (che >> 23);
		shang = ww / jz;
		yu = ww % jz;
		thres = (jz >> 1);
		if(yu > thres || (yu == thres && (shang%2))){
			shang++;
			if(shang >> 23) zgw++;
		}
		ws = shang & 0x007FFFFF;
	}
	ans = ans | ((zgw + 127) << 23);
	ans = ans | ws;
	return ans;
}

unsigned float_times64(unsigned uf) {
	unsigned exp_ = uf & 0x7F800000;
	unsigned ws = uf & 0x7FFFFF;
	unsigned ws_ = ws;
	int ydcs = 0x3800000;
	if(exp_ == 0x7F800000) return uf;
	else if(exp_ >= 0x7C800000){
		return (uf & 0x80000000) + 0x7F800000;
	}
	else if(exp_){
		return uf + 0x3000000;
	}
	else if(ws < 0x20000){
		//cout << "uf =  " << uf <<  ",ws = " << ws << endl;
		return uf + 63 * ws;
	}
	else{
		while(ws_ < 0x800000){
			ws_ = (ws_ << 1);
			ydcs -= 0x800000;
		}
		ws_ -= 0x800000;
		return uf - ws + ws_ + ydcs;
	}
}


