/* -----------------------------------------------------------------------------

FILE:              bankacct.h

DESCRIPTION:       Header file for my project. Contains constants, structures, and classes needed for main program

COMPILER:          g++ with c++ 11

MODIFICATION HISTORY:

Author                    Date               Version
---------------           ----------         --------------
Alexander Novotny         2016-10-13         1.0.0

----------------------------------------------------------------------------- */

#ifndef __BANKACCT_H__
#define __BANKACCT_H__

//Semvers
#define VERSION "0.0.1" 

#define FIRST_NAME_LENGTH 50
#define LAST_NAME_LENGTH 50
#define ACC_NUM_LENGTH 5
#define PASS_LENGTH 6

//This stuff defines how the menus looks (in case I want to change it later)
//I'm also doing this for purposes of literacy
//MIN_ and MAX_ defines how much the name and balance columns can stretch
//You shouldn't really mess around with MIN_
#define MIN_NAME 17
#define MAX_NAME 28
#define XTRA_NAME MAX_NAME - MIN_NAME
#define MIN_BAL 7
#define MAX_BAL 15
#define XTRA_BAL MAX_BAL - MIN_BAL
//This stuff should be constant - it's the width of each column
#define ACC_COL 7
#define SSN_COL 9
#define PHO_COL 12
#define MAX_VAR XTRA_NAME + XTRA_BAL
#define MIN_ROW 11
#define MAX_ROW 40
#define UI_ROWS 6

//Now this stuff is for the account menu
#define ACC_MIN_WIDTH 46
#define ACC_MIN_HEIGHT 10
#define ACC_MAIN_MIN 30
#define ACC_SEPARATION 7

//Transfer menu
#define TRANS_MIN_WIDTH 80
#define TRANS_MIN_HEIGHT 10
#define TRANS_MID_COL 10

//New Account Menu
#define NEWACC_LEFTSHIFT 20

using namespace std;

struct Account {
	char first[FIRST_NAME_LENGTH + 1];
	char last[LAST_NAME_LENGTH + 1];
	char middle;
	unsigned int social;
	unsigned int area;
	unsigned int phone;
	double balance;
	char number[ACC_NUM_LENGTH + 1];
	char password[PASS_LENGTH + 1];
	//Length of full name (including two spaces and a .)
	unsigned int nameLength;
};


class WriteOnShutdown {
	private:
		const char* filename;
		vector<Account>* database;
	public:
		WriteOnShutdown(char* a, vector<Account>* b) : filename(a), database(b) {}
		
		/* -----------------------------------------------------------------------------
		FUNCTION:          ~WriteOnShutdown()
		DESCRIPTION:       Destructor for WriteOnShutdown. When WriteOnShutdown gets deleted, 
                                   it writes the database to the specified output file
		RETURNS:           Void function
		----------------------------------------------------------------------------- */
		~WriteOnShutdown() {
			ofstream out(filename);
			for(Account& acc : *database) {
				out << acc.first << endl
				    << acc.last << endl
					<< acc.middle << endl
					<< acc.social << endl
					<< acc.area << endl
					<< acc.phone << endl
					<< acc.balance << endl
					<< acc.number << endl
					<< acc.password << endl << endl;
			}
			getch();
		}
};
#endif
