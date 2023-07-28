#include "rtl_glue.h"
#include "stateMachine.h"
#include "_kmp.h"


char* adjestPatternLocation(char* inputString);
int isRedundantChar(char Char, char* cmpCharArray, int lastIndex);
int* countNumOfRules(int* fail, char* pattern);

/*Generate fail function for the input pattern
 * pattern:      The given pattern to generate fail function.
 * Return value: It will return the pointer that point to the fail function array.*/
int* failFunction(char* pattern)
{
  int* fail;
  int patternSize;
  int i;
  int s;
  patternSize = strlen(pattern);
  fail = rtlglue_malloc((patternSize+1)*sizeof(int));
  if(fail==NULL){
     rtlglue_printf("fail function malloc fail!!\n");
     //exit(1);
     return NULL;
  }
  fail[0]=0;
  fail[1]=0;
  for(i=2; i<=patternSize; i++){
      s = fail[i-1];
      while(s>0){
          if(pattern[i-1]==pattern[s])
            break;
          s = fail[s];
      }
      fail[i]=s+1; 
  }

  return fail;
}

/*Determine whether a given char is in an character array.
 * Char:         The given character to find in character array cmpCharArray.
 * cmpCharArray: We search the given charactor in this array.
 * lastIndex:    This is the last index value that we will search in the cmpCharArray.*/
int isRedundantChar(char Char, char* cmpCharArray, int lastIndex)
{
   int i;
   for(i = 0 ; i < lastIndex ; i++){
     if(Char == cmpCharArray[i])
        return 1;
   } 
   return 0;
}

/*This function counts the numbers of transition rules for each state of state machine 
 * generated by kmp.
 * fail:    the pointer points to the fail function.
 * pattern: the input pattern that produces fail function.
 * Return value: This is the pointer that points to an int array which cotains the number
 *          of rules needed by each sstate*/
int* countNumOfRules(int* fail, char* pattern)
{
    int numOfStates;
    int* NumOfRules;
    int countRules;
    char* cmpChar;
    int cmpCharIndex;    
    int i, s;

    numOfStates = strlen(pattern)-1;
    cmpChar = (char*)rtlglue_malloc(strlen(pattern)*sizeof(char));
    NumOfRules = (int*)rtlglue_malloc(numOfStates*sizeof(int));
    for(i=0; i<numOfStates ; i++){
       countRules = 0;
       cmpCharIndex = 0; 
       //success link, it is next state char.
       countRules++;
       cmpChar[cmpCharIndex++]=pattern[i+1];

       //fail link, all distinct chars along the path of fail links.
       s = fail[i+1];
       while(s>0){
         if(isRedundantChar(pattern[s], cmpChar, cmpCharIndex) == 0){
            cmpChar[cmpCharIndex++] = pattern[s];
            countRules++;
         }
         s = fail[s];
       }
       
       //All the other chars
        countRules++;

       NumOfRules[i]=countRules;
    }
    rtlglue_free(cmpChar);
    return NumOfRules;

}

/* Generate a state machine for an input pattern.
 * inputPattern: The input pattern to produce the state machine.
 * Retern value: The pointer points to the state machine generated for the pattern.*/
SM* kmp_MakeStateMachine(char* inputPattern)
{
   int patternSize;
   int i, s;
   int* numOfRulesArray;
   int* fail;
   char* cmpCharArray; //use this to record chars that have already appeared.
   int cmpCharIndex;
   int ruleIndex;
   char* pattern;
   State* tmpState;
   SM* sm;

   tmpState = NULL;
   pattern = adjestPatternLocation(inputPattern);   
   fail = failFunction(pattern);

   patternSize =  strlen(pattern)-1;
   cmpCharArray = (char*)rtlglue_malloc(patternSize*sizeof(char));
   sm = (SM*)rtlglue_malloc(1*sizeof(SM));
   sm->numOfStates = patternSize;
   sm->states =(State**)rtlglue_malloc(((patternSize+STATE_ARRAY_CAPACITY-1)/STATE_ARRAY_CAPACITY)*sizeof(State*));
   numOfRulesArray = countNumOfRules(fail, pattern);

   for(i = 0; i < patternSize ; i++){
	   if(i%STATE_ARRAY_CAPACITY==0){
		tmpState =  (State*)rtlglue_malloc(STATE_ARRAY_CAPACITY*sizeof(State));
		if(!tmpState)
			rtlglue_printf("State malloc fail! Function %s, Line %d\n",__FUNCTION__, __LINE__);

		sm->states[i/STATE_ARRAY_CAPACITY] = tmpState;
	   }
       tmpState[i%STATE_ARRAY_CAPACITY].stateID = i;
       tmpState[i%STATE_ARRAY_CAPACITY].numOfRules = numOfRulesArray[i];
       tmpState[i%STATE_ARRAY_CAPACITY].transRules =(Rule*)rtlglue_malloc(numOfRulesArray[i]*sizeof(Rule));
       ruleIndex = 0;
       cmpCharIndex = 0;
      //success link
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cmpChar = pattern[i+1];
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].not = 0;

       if(i==patternSize-1){
           tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cpu = 1;
           tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].jmpState = fail[patternSize+1]-1;
       }
       else{
           tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cpu = 0;
           tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].jmpState = i+1;
       }
       cmpCharArray[cmpCharIndex++] = pattern[i+1];
       ruleIndex++;

      //recursive fail link
      s = fail[i+1];
      while(s>0){
          if(isRedundantChar(pattern[s], cmpCharArray, cmpCharIndex)== 0){
               tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cmpChar = pattern[s];
               tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].not = 0;
               tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cpu = 0;
               tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].jmpState = s;
               cmpCharArray[cmpCharIndex++] = pattern[s];
               ruleIndex++;
          }
          s = fail[s];
      }


       //All other chars
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cmpChar = pattern[i+1];
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].not = 1;
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].cpu = 0;
       tmpState[i%STATE_ARRAY_CAPACITY].transRules[ruleIndex].jmpState = 0;

       //exchange last two transition rules
/*Move this to ruleTransform.c stateToRule function
       sm->states[i].transRules[ruleIndex].cmpChar = sm->states[i].transRules[ruleIndex-1].cmpChar;
       sm->states[i].transRules[ruleIndex].not = sm->states[i].transRules[ruleIndex-1].not;
       sm->states[i].transRules[ruleIndex].cpu = sm->states[i].transRules[ruleIndex-1].cpu;
       sm->states[i].transRules[ruleIndex].jmpState = sm->states[i].transRules[ruleIndex-1].jmpState;

       sm->states[i].transRules[ruleIndex-1].not = 1;
       sm->states[i].transRules[ruleIndex-1].cpu = 0;
       sm->states[i].transRules[ruleIndex-1].jmpState = 0;
*/
   }
   rtlglue_free(fail);
   rtlglue_free(numOfRulesArray);
   rtlglue_free(cmpCharArray);
   rtlglue_free(pattern);
   return sm;
}


/*This function print out the fail function for the input pattern.*/
void showFailFunction(char* inputPattern)
{
  int i;
  char* pattern; 
  int sizeOfFail;
  int* fail;
  pattern = adjestPatternLocation(inputPattern);
  fail = failFunction(pattern);
  sizeOfFail = strlen(pattern);
  for(i=0 ; i<sizeOfFail ; i++){
    rtlglue_printf("fail[%d] = %d\n", i, fail[i]);
  }

  rtlglue_free(pattern);
  rtlglue_free(fail);
}

/* We would like to adjest the given string pattern which is inputString
 * from [0..m] to [1..m+1]. This would help to program the kmp algorithm. */
char* adjestPatternLocation(char* inputString)
{
   char* pattern;

   pattern = rtlglue_malloc((strlen(inputString)+1)*sizeof(char));
   if(pattern == NULL){
     rtlglue_printf("adjestPatternLocation malloc fail!!\n");
     //exit(1);
     return NULL;
   }
   pattern[0] = 's';
   strcpy(pattern+1, inputString);
   return pattern;
}

