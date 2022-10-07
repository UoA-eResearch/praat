/* Index.cpp
 *
 * Copyright (C) 2005-2022 David Weenink
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 djmw 20050724
 djmw 20061212 Changed info to Melder_writeLine<x> format.
 djmw 20070102
 djmw 20071012 Added: o_CAN_WRITE_AS_ENCODING.h
 djmw 20110304 Thing_new
*/

#include <time.h>
#include "Index.h"
#include "NUM2.h"
#include "Permutation.h"

#include "oo_DESTROY.h"
#include "Index_def.h"
#include "oo_COPY.h"
#include "Index_def.h"
#include "oo_EQUAL.h"
#include "Index_def.h"
#include "oo_CAN_WRITE_AS_ENCODING.h"
#include "Index_def.h"
#include "oo_WRITE_TEXT.h"
#include "Index_def.h"
#include "oo_WRITE_BINARY.h"
#include "Index_def.h"
#include "oo_READ_TEXT.h"
#include "Index_def.h"
#include "oo_READ_BINARY.h"
#include "Index_def.h"
#include "oo_DESCRIPTION.h"
#include "Index_def.h"

static const conststring32 undefinedClassLabel = U"";
Thing_implement (Index, Daata, 0);

void structIndex :: v1_info () {
	structDaata :: v1_info ();
	MelderInfo_writeLine (U"Number of items: ", our numberOfItems);
}

/*
	Sort strings that may have numeric substrings:
	string    := <alphaPart> | <numPart>
	alphaPart := <alphas> | <alphas><numPart>
	numPart   := <nums> | <nums><alphaPart>
	
	numeric part should sort numerically:
	0a, 1a, 2a, 11a
	00a, 0a, a
	00a1, 00a2, 00a11, 0b0

*/
typedef struct structSTRVECSorter *STRVECSorter;
struct structSTRVECSorter {
	struct structStringsInfo {		
		private:
		struct structStringInfoData {
			double number;	// interpretation of part of alpha as number
			integer alphaPosStart; // start positions of alphaPart; alphaPosEnd is fixed at the end U'\0'
			integer numPosStart; // start of num part else 0
			integer numPosDot; // numPosStart < numPosDot < numPosEnd if a dot occurs in the num part and breakAtDecimalPoint==true
			integer numPosEnd; // >= numPosStart if numPosStart > 0 else undefined
			integer length; 
			mutablestring32 alpha;
			char32 save0;
		};	
		using STRINFOVEC = vector<struct structStringInfoData>;
		using autoSTRINFOVEC = autovector<struct structStringInfoData>;
		using constSTRINFOVEC = constvector <struct structStringInfoData>;
		typedef struct structStringInfoData *stringInfo; 
		
		autoSTRINFOVEC stringsInfoDatavector;
		bool breakAtDecimalPoint = true;
		
		/* always use maskAlphaTrailer do something and then unmaskAlphaTrailer */
		void maskAlphaTrailer (stringInfo info) { //  save the char32 after number part and replace by U'\0'
			if (info -> numPosStart > 0 && info -> numPosEnd < info -> length) {  
				info -> save0 = info -> alpha [info -> numPosEnd]; // save position after last digit
				info -> alpha [info -> numPosEnd] = U'\0'; // mark number part only
			}
		}
	
		void unmaskAlphaTrailer (stringInfo info) { // restore the masking!
			if (info -> numPosStart > 0 && info -> numPosEnd < info -> length)
				info -> alpha [info -> numPosEnd] = info -> save0; 
		}
		void maskNumTrailer (stringInfo info) {
			if (info -> numPosStart > 0) {
				info -> save0 = info -> alpha [info -> numPosStart - 1];
				info -> alpha [info -> numPosStart - 1] = U'\0';
			}
		}
		void unmaskNumTrailer (stringInfo info) {
			if (info -> numPosStart > 0)
				info -> alpha [info -> numPosStart - 1] = info -> save0;
		}
		
		void setNumPart (stringInfo info) {
			setNumPosStart (info);
			if (info -> numPosStart > 0) {
				setNumPosEndAndDot (info);
				maskAlphaTrailer (info); // save position after last digit and put '\0'
				info -> number = Melder_atof (info -> alpha + info -> numPosStart - 1);
				unmaskAlphaTrailer (info); //
			}
		}
		/*
			Find the first and last index of the first numeric part in a string.
			The allowed numeric part is either <d> or <d>.<d>, where <d> a sequence of one or more digits
			Examples with breakAtDecimalPoint
						false 		true
			s1d 		-> 2,2		2,2
			2.222		-> 1,5		1,1
			abc123		-> 4,6		4,6
			a.3			-> 3,3		3,3
			a..3		-> 4,4		4,4
			a..3.		-> 4,4		4,4
			b33.33.4	-> 2,6		2,3
		*/
		void setNumPosStart (stringInfo info) {
			const char32 *p = info -> alpha + info -> alphaPosStart - 1;
			while (*p != U'\0' && (*p < U'0' || *p > U'9'))
				p ++;
			if (*p != U'\0')
				info -> numPosStart = 1 + p - info -> alpha;
			else
				info -> numPosStart = 0;
		}
		void setNumPosEndAndDot (stringInfo info) {
			if (info -> alphaPosStart < 0)
				return;
			const char32 *pstart = info -> alpha + info -> numPosStart - 1;
			const char32 *decimalPoint = 0, *p = pstart;
			while (*p != U'\0') {
				if (*p == U'.') {
					if (decimalPoint) { // second occurence: stop
						p --;
						break;
					} else {
						if (breakAtDecimalPoint)
							break;
						else
							decimalPoint = p;
					}
				} else if (*p < U'0' || *p > U'9') {
					break;
				}
				p ++;
			}
			if (p - decimalPoint == 1) // no decimal point at end!
				p --;
			info -> numPosDot = ( (breakAtDecimalPoint || decimalPoint) ? 0 : info -> numPosStart + decimalPoint - pstart);
			const integer relPos = p - pstart - 1;
			info -> numPosEnd = info -> numPosStart + relPos;
		}
		stringInfo getStringInfo (integer index) {
			Melder_assert (index > 0 && index <= stringsInfoDatavector.size);
			return & stringsInfoDatavector [index];
		}
				
		public:
		void init (constSTRVEC const& strings) { // strings are unicized!
			stringsInfoDatavector = newvectorraw <struct structStringInfoData> (strings.size);
			for (integer i = 1; i <= strings.size; i++) {
				stringInfo info  = getStringInfo (i);
				info -> alphaPosStart = 1;
				info -> length = str32len (strings [i]);
				info -> alpha = (char32 *)_Melder_calloc (info -> length + 1, sizeof (char32));
				str32cpy (info -> alpha, strings [i]); // adds the U'\0'
				info -> number = -1.0; 
			}
		}
		integer setAlphaPosStartAtNextLevel (constINTVEC set) {
			integer notAtEnd = 0;
			for (integer i = 1; i <= set.size; i ++) {
				stringInfo info = getStringInfo (set [i]);
				if (info -> numPosStart == 0)
					info -> alphaPosStart = info -> length + 1; // at \0
				else if (info -> alphaPosStart <= info -> numPosStart) {
					info -> alphaPosStart = info -> numPosEnd + 1;
					if (info -> alphaPosStart <= info -> length)
						notAtEnd ++;
				}
			}
			return notAtEnd;
		}
		integer setAlphaPosStartAfterNumPosEnd (constINTVEC set) {
			integer notAtEnd = 0;
			for (integer i = 1; i <= set.size; i ++) {
				stringInfo info = getStringInfo (set [i]);
				if (info -> numPosStart == 0)
					info -> alphaPosStart = info -> length + 1; // at \0
				else if (info -> numPosEnd <= info -> length) {
					info -> alphaPosStart = info -> numPosEnd + 1;
					notAtEnd ++;
				}
			}
			return notAtEnd;
		}
		integer setAlphaPosStartAtNumPosStart (constINTVEC set) {
			integer notAtEnd = 0;
			for (integer i = 1; i <= set.size; i ++) {
				stringInfo info = getStringInfo (set [i]);
				if (info -> numPosStart == 0)
					info -> alphaPosStart = info -> length + 1; // at \0
				else if (info -> alphaPosStart <= info -> numPosStart) {
					info -> alphaPosStart = info -> numPosStart;
					notAtEnd ++;
				}
			}
			return notAtEnd;
		}
		void updateNumPart (constINTVEC set) {
			for (integer i = 1; i <= set.size; i++) {
				stringInfo info = getStringInfo (set [i]);
				setNumPart (info);
			}
		}
			
		bool separateAlphaAndNumSets (constINTVEC const& subset, autoINTVEC & numStartSet, autoINTVEC & alphaStartSet) {
			integer num = 0, alpha = 0, done = 0;
			for (integer i = 1; i <= subset.size; i ++) {
				integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				if (info -> numPosStart > 0 && info -> numPosStart == info -> alphaPosStart)
						numStartSet.insert (++ num, irow);
				else {
					if (info -> alphaPosStart > 0)
						alphaStartSet.insert (++ alpha, irow);
					else
						done ++;
				}
			}
			numStartSet.resize (num);
			alphaStartSet.resize (alpha);
			return done != subset.size;
		}
		
		autoVEC extractNums (constINTVEC const& subset) {
			autoVEC numbers = raw_VEC (subset.size);
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				numbers [i] = info -> number;
			}
			return numbers;
		}
		autoSTRVEC extractAlphas (constINTVEC const& subset) {
			autoSTRVEC alphas (subset.size);
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				maskNumTrailer (info);
				alphas [i] = Melder_dup (info -> alpha + info -> alphaPosStart - 1);
				unmaskNumTrailer (info);
			}
			return alphas;
		}
		autoSTRVEC extractAlphaPart (constINTVEC const& subset) {
			autoSTRVEC alphaPart (subset.size);
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				alphaPart [i] = Melder_dup (info -> alpha + info -> alphaPosStart - 1);
			}
			return alphaPart;
		}
		
		autostring32 extractNumPartAsString (integer index) {
				stringInfo info = getStringInfo (index);
				maskAlphaTrailer (info);
				autostring32 result = Melder_dup (info -> alpha + info -> alphaPosStart - 1);
				unmaskAlphaTrailer (info);
				return result;
		}

		autoSTRVEC extractAlphaPartFromNumPart (constINTVEC const& subset, integer& numberOfNonEmpty)  {
			autoSTRVEC alphaPart (subset.size);
			numberOfNonEmpty = 0;
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				if (info -> numPosStart > 0 && info -> numPosEnd < info -> length) {
					alphaPart [i] =  Melder_dup (info -> alpha + info -> numPosEnd + 1);
					numberOfNonEmpty ++;
				} else
					alphaPart [i] =  Melder_dup (U"");
			}
			return alphaPart;
		}
		
		autoSTRVEC extractNumPartFromAlphaPart (constINTVEC const& subset, integer& numberOfNonEmpty)  {
			autoSTRVEC numPart (subset.size);
			numberOfNonEmpty = 0;
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				if (info -> numPosStart > 0) {
					maskAlphaTrailer (info);
					numPart [i] =  Melder_dup (info -> alpha + info -> numPosStart - 1);
					unmaskAlphaTrailer (info);
					numberOfNonEmpty ++;
				} else
					numPart [i] =  Melder_dup (U"");
			}
			return numPart;
		}
		
		void extractNumPartOnly (constINTVEC const& subset, autoSTRVEC & alphas, autoSTRVEC & alphaPart) {
			
			for (integer i = 1; i <= subset.size; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				if (info -> alphaPosStart > 0) {
					alphaPart.insert (i, info -> alpha + info -> alphaPosStart - 1);
					maskNumTrailer (info);
					alphas.insert (i, info -> alpha + info -> alphaPosStart - 1);
					unmaskNumTrailer (info);
				} else {
					alphaPart.insert (i, U"");
					alphas.insert (i, U"");
				}
			}
		}
		
		autoPermutation sortAsAlpha (constINTVEC subset) {
			const integer numberOfStrings = subset.size;
			autoSTRVEC alphas (numberOfStrings);
			for (integer i = 1; i <= numberOfStrings; i ++) {
				const integer irow = subset [i];
				stringInfo info = getStringInfo (irow);
				alphas.insert (i, info -> alpha + info -> alphaPosStart - 1);
			}
			autoPermutation p = Permutation_create (numberOfStrings, true); // TODO CHECK ?
			INTVECindex (p -> p.get(), alphas.get());
			return p;
		}
	};
	
	struct structSTRVECIndex {
		autoSTRVEC classes; // the number of different strings in the originating strvec.
		autoINTVEC classesIndex; // has length of the originating strvec 
		
		void unicize (constSTRVEC const& v) {
			integer count = 1;
			classesIndex = raw_INTVEC (v.size);
			autoPermutation p = Permutation_create (v.size, true);
			INTVECindex (p -> p.get(), v);
			classes.insert (count, v [p -> p [1]]);
			classesIndex [1] = count;
			for (integer i = 2; i <= v.size; i ++) {
				const integer index = p -> p [i];
				conststring32 str = v [index];
				if (Melder_cmp (classes [count].get(), str) != 0)
					classes.insert (++ count, str);
				classesIndex [i] = count;
			}
		}
	};

private:
	INTVEC strvecIndex; // link to the alphabetically sorted index of the strvec
	STRVEC strings;  // link to the unicized strvec, these will be sorted
	autoPermutation stringsSorting; // the sorting index of these strings 
	bool breakAtDecimalPoint;
	struct structStringsInfo stringsInfo;
	struct structSTRVECIndex STRVECIndex;
	
	void init (constSTRVEC const& v, bool breakAtTheDecimalPoint ) {
		try {
			breakAtDecimalPoint = breakAtTheDecimalPoint;
			STRVECIndex.unicize (v);
			strings = STRVECIndex.classes.get();
			stringsSorting = Permutation_create (STRVECIndex.classes.size, true);
			strvecIndex = STRVECIndex.classesIndex.get();
		} catch (MelderError) {
			Melder_throw (U"Cannot init the structStringsInfo.");
		}
	 }

 public:
	// the elements of set sets must always refer to the position in the structSTRVECIndex::classes
	autoPermutation sortAlphaPartSet (constINTVEC const& set, integer level) {
		Melder_assert (set.size > 0);
		autoPermutation palpha = Permutation_create (set.size, true);
		autoSTRVEC alphaPart = stringsInfo.extractAlphaPart (set);
		autoSTRVEC alphas = stringsInfo.extractAlphas (set);
		if (level > 1) // already sorted by init
			INTVECindex (palpha -> p.get(), alphaPart.get()); // we want s01 before s1!
		
		autoINTVEC equalsSet = raw_INTVEC (set.size);
		integer startOfEquals = 1;
		conststring32 value = alphas [palpha -> p [1]].get(); // the smallest 
		for (integer i = 2; i <= set.size; i ++) {
			conststring32 current = alphas [palpha -> p [i]].get();
			if (Melder_cmp (current, value) != 0) {
				const integer numberOfEquals = i - startOfEquals;
				if (numberOfEquals > 1) {
					for (integer j = 1; j <= numberOfEquals; j ++)
						equalsSet [j] = set [palpha -> p [startOfEquals + j - 1]];
					equalsSet.resize (numberOfEquals);
					if (stringsInfo.setAlphaPosStartAtNumPosStart (equalsSet.get())) {
						autoPermutation pequals = sortNumPartSet (equalsSet.get(), level);
						Permutation_permuteSubsetByOther_inout (palpha.get(), equalsSet.get(), pequals.get());
					}
				}
				value = current;
				startOfEquals = i;
			}
		}
		return palpha;
	}
	
	/* the elements of set must always refer to the position in the structSTRVECIndex::classes
		Precondition: alphabetical sorting before sortNumPartSet is called.
		strings {"4b","04b","4a","004a","1","d","c","004b" } were already sorted as {"004a","004b","04b","1","4a","4b","c","d"},
		we want to sort them as {"1", "004a","004b","4a","04b","4b","c","d"}
		if the strings before the alpha reduce to the same number, they are already in the correct order.
			("004a", "4a", "04b", "4b")
		strings <num><alpha><num> {"4b1","4b01"} 
	*/
	autoPermutation sortNumPartSet (constINTVEC const& set, integer level) {
		/*
			Convert numeric part to numbers
		*/
		const integer numberOfStrings = set.size;
		autoVEC numbers = stringsInfo.extractNums (set);
		autoPermutation	pset = Permutation_create (numberOfStrings, true);
		INTVECindex (pset -> p.get(), numbers.get());
		/*
			Find numbers that are equal but have different number of digits
		*/
		//autoSTRVEC alphas = stringsInfo.extractAlphas (set);;
		autoINTVEC equalsSet = raw_INTVEC (numberOfStrings);

		integer startOfEquals = 1;
		double value = numbers [pset -> p [1]]; // the smallest number
		for (integer i = 2; i <= numberOfStrings; i++) {
			const double current = numbers [pset -> p [i]];
			if (current != value) {
				const integer numberOfEquals = i - startOfEquals;
				if (numberOfEquals > 1) {
					autoSTRVEC numbers_strings (numberOfEquals);
					for (integer j = 1; j <= numberOfEquals; j ++) {
						const integer index = set [pset -> p [startOfEquals + j - 1]];
						equalsSet [j] = startOfEquals + j - 1;
						numbers_strings[j] = stringsInfo.extractNumPartAsString (index).move();
					}
					equalsSet.resize (numberOfEquals);
					// now sort the numbers_strings
					autoPermutation pnumbers_strings = Permutation_create (numberOfEquals, true);
					INTVECindex (pnumbers_strings -> p.get(), numbers_strings.get());
					Permutation_permuteSubsetByOther_inout (pset.get(), equalsSet.get(), pnumbers_strings.get()); 
					// get the numPart and sort it 
					if (stringsInfo.setAlphaPosStartAfterNumPosEnd (pnumbers_strings -> p.get())) {
						autoPermutation pequals = sortAlphaPartSet (equalsSet.get(), level + 1); // 
						Permutation_permuteSubsetByOther_inout (pset.get(), equalsSet.get(), pequals.get());
					}
				}
				value = current;
				startOfEquals = i;
			}
		}
		return pset;
	}
	
	void sort (constSTRVEC const& v,  bool breakAtTheDecimalPoint, kStrings_sorting sorting) {
		init (v, breakAtTheDecimalPoint);
		if (sorting == kStrings_sorting::ALPHABETICAL) {
			 ;
		} else if (sorting == kStrings_sorting::NUMERICAL_PART) {
			stringsInfo.init (strings);
			stringsInfo.updateNumPart (stringsSorting -> p.get());
			autoINTVEC numStartSet, alphaStartSet;
			autoPermutation p = Permutation_create (strings.size, true);
			Melder_require (stringsInfo.separateAlphaAndNumSets (p->p.get(), numStartSet, alphaStartSet),
				U"error");
			if (numStartSet.size > 0) {
				autoPermutation pnumPart = sortNumPartSet (numStartSet.get(), 1);
				Permutation_permuteSubsetByOther_inout (stringsSorting.get(), numStartSet.get(), pnumPart.get());
			}
			if (alphaStartSet.size > 0) {
				autoPermutation palphaPart = sortAlphaPartSet (alphaStartSet.get(), 1);
				Permutation_permuteSubsetByOther_inout (stringsSorting.get(), alphaStartSet.get(), palphaPart.get());
			}
			if (numStartSet.size > 0 && alphaStartSet.size > 0)
			Permutation_moveElementsToTheFront (stringsSorting.get(), numStartSet.get());
		}
	 }
	 
	autoStringsIndex sortindex (constSTRVEC const& v,  bool breakAtTheDecimalPoint, kStrings_sorting sorting) {
		sort (v, breakAtTheDecimalPoint, sorting);
		autoStringsIndex me = StringsIndex_create (v.size);
		for (integer i = 1; i <= strings.size; i ++) {
			conststring32 classi = strings [stringsSorting -> p [i]];
			autoSimpleString ss = SimpleString_create (classi);
			my classes -> addItem_move (ss.move());
		}
		for (integer i = 1; i <= v.size; i ++)
			my classIndex [i] = ( sorting == kStrings_sorting::ALPHABETICAL ? 
				strvecIndex [i] : stringsSorting -> p [strvecIndex [i]]);
		return me;
	 }
};
 
void Index_init (Index me, integer numberOfItems) {
	Melder_require (numberOfItems > 0,
		U"The index should not be empty.");
	my classes = Ordered_create ();
	my numberOfItems = numberOfItems;
	my classIndex = zero_INTVEC (numberOfItems);
}

autoIndex Index_extractPart (Index me, integer from, integer to) {
	try {
		if (from == 0)
			from = 1;
		if (to == 0)
			to = my numberOfItems;
		Melder_require (from <= to && from > 0 && to <= my numberOfItems,
			U"Range should be in interval [1,", my numberOfItems, U"].");
		
		autoIndex thee = Data_copy (me);
		thy numberOfItems = to - from + 1;
		
		for (integer i = 1; i <= thy numberOfItems; i ++)
			thy classIndex [i] = my classIndex [from + i - 1];
		return thee;
	} catch (MelderError) {
		Melder_throw (me, U": part not extracted.");
	}
}

Thing_implement (StringsIndex, Index, 0);

autoStringsIndex StringsIndex_create (integer numberOfItems) {
	try {
		autoStringsIndex me = Thing_new (StringsIndex);
		Index_init (me.get(), numberOfItems);
		return me;
	} catch (MelderError) {
		Melder_throw (U"StringsIndex not created.");
	}
}


void MelderString_appendPart (MelderString *me, conststring32 source, integer first, integer last) {
	const integer length = str32len (source);
	Melder_assert (first <= length);
	if (last == 0)
		last = length;
	last = std::min (last, length);
	for (integer ichar = first; ichar <= last; ichar ++)
			MelderString_appendCharacter(me, source [ichar - 1]); // 0-based
	MelderString_appendCharacter (me, U'\0');
}

void MelderString_copyPart (MelderString *me, conststring32 source, integer first, integer last) {
	MelderString_empty (me);
	MelderString_appendPart (me, source, first, last);
}


void INTVECindex_num_alpha (INTVEC const& target, constSTRVEC const& v, kStrings_sorting sorting) {
	INTVECindex (target, v);
	if (sorting == kStrings_sorting::ALPHABETICAL)
		return;
	else 
		return; // TODO
}

/* copying of strings could be avoided by using a autovector<char32** ? */
autoStringsIndex StringsIndex_createFrom_STRVEC (constSTRVEC const& strvec, kStrings_sorting sorting, bool breakAtDecimalPoint) {
	try {
		struct structSTRVECSorter sorter;
		autoStringsIndex me =  sorter.sortindex (strvec, breakAtDecimalPoint, sorting);	
		return me;
	} catch (MelderError) {
		Melder_throw (U"Could not create StringsIndex from STRVEC.");
	}
}

autoStringsIndex Strings_to_StringsIndex (Strings me); // TO remove all this

autoStringsIndex Strings_to_StringsIndex2 (Strings me, kStrings_sorting sorting) {
	try {
		if (Melder_debug == -7) {
		autoStringsIndex thee = StringsIndex_createFrom_STRVEC (my strings.get(), sorting, false);
		return thee;
		}
		return Strings_to_StringsIndex (me);
	} catch (MelderError) {
		Melder_throw (me, U": no StringsIndex created.");
	}
}

integer Index_getClassIndexFromItemIndex (Index me, integer itemIndex) {
	integer result = 0;
	if (itemIndex >= 0 && itemIndex <= my numberOfItems)
		result = my classIndex [itemIndex];
	return result;
}

int StringsIndex_getClassIndexFromClassLabel (StringsIndex me, conststring32 klasLabel) {
	for (integer i = 1; i <= my classes->size; i ++) {
		const SimpleString ss = (SimpleString) my classes->at [i];   // FIXME cast
		if (Melder_equ (ss -> string.get(), klasLabel))
			return i;
	}
	return 0;
}

conststring32 StringsIndex_getClassLabelFromClassIndex (StringsIndex me, integer klasIndex) {
	conststring32 result = undefinedClassLabel;
	if (klasIndex > 0 && klasIndex <= my classes -> size) {
		const SimpleString ss = (SimpleString) my classes->at [klasIndex];   // FIXME cast
		result = ss -> string.get();
	}
	return result;
}

conststring32 StringsIndex_getItemLabelFromItemIndex (StringsIndex me, integer itemNumber) {
	conststring32 result = undefinedClassLabel;
	if (itemNumber > 0 && itemNumber <= my numberOfItems) {
		const integer klas = my classIndex [itemNumber];
		const SimpleString ss = (SimpleString) my classes->at [klas];   // FIXME cast
		result = ss -> string.get();
	}
	return result;
}

integer StringsIndex_countItems (StringsIndex me, integer iclass) {
	integer sum = 0;
	for (integer i = 1; i <= my numberOfItems; i ++) {
		if (my classIndex [i] == iclass)
			sum ++;
	}
	return sum;
}

void StringsIndex_sortNumerically (StringsIndex me) {
	try {
		const integer numberOfClasses = my classes -> size;
		autoSTRVEC numberstrings (numberOfClasses);
		autoVEC numbers = raw_VEC (numberOfClasses);
		for (integer i = 1; i <= numberOfClasses; i ++) {
			const SimpleString ss = (SimpleString) my classes->at [i];
			numberstrings [i] = Melder_dup (ss -> string.get());
			numbers [i] = Melder_atof (ss -> string.get());
		}
		autoPermutation p = Permutation_create (numberOfClasses, true);
		NUMsortTogether (numbers.get(), p -> p.get());
		for (integer i = 1; i <= numberOfClasses; i ++) {
			autoSimpleString ss = SimpleString_create (numberstrings [i].get());
			my classes-> replaceItem_move (ss.move(), p -> p [i]);
		}
		for (integer item = 1; item <= classIndex->size; item ++)
			my classIndex [item] = p -> p [my classIndex [item]];
	} catch (MelderError) {
		Melder_throw (me, U": could not be sorted numerically.");
	}
}

/* End of Index.cpp */
