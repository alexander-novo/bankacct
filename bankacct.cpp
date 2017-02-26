/* -----------------------------------------------------------------------------

	FILE:              bankacct.cpp
	DESCRIPTION:       A bank account simulator which keeps track of bank accounts and lets you manage them
	COMPILER:          Built on g++ with c++11
	Exit Codes:
		- 0: All good
		- 1: Could not load Database file
	LIBRARIES:
		- NCursesW: Used for the user interface. W form for wide character support
	
	MODIFICATION HISTORY:
	Author                  Date               Version
	---------------         ----------         --------------
	Alexander Novotny       09/13/2016         0.0.1
	Alexander Novotny       10/13/2016         1.0.0
----------------------------------------------------------------------------- */

#include <ncurses.h>
#include <locale.h> //To set locale to UTF-8
#include <cstring>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm> //For std::sort
#include <regex>
#include <iomanip>
#include "bankacct.h"

using namespace std;

void mainMenu(vector<Account>*);
void drawMainMenu(vector<Account>*, unsigned int, unsigned int);
void printHeading(unsigned int, char const*);

void displayAccount(vector<Account>*, unsigned int);

void deposit(vector<Account>*, unsigned int);
void withdraw(vector<Account>*, unsigned int);
void transfer(vector<Account>*, unsigned int);
Account* transferAccount(vector<Account>*, unsigned int);
void transferAmmount(Account*, Account*);
bool close(vector<Account>*, unsigned int);
bool verify(Account*);

void openAccount(vector<Account>*);

void createReport(vector<Account>*);

char* loadDatabase(vector<Account>*);
void getDBFileName(char[50]);

void initNcurses();
unsigned int numPlaces(long long);
void onExit();


/* -----------------------------------------------------------------------------
FUNCTION:          main()
DESCRIPTION:       Initialises ncurses, loads and sorts database, 
                   prepares database for writing on shutdown, and then runs main menu
RETURNS:           See Exit Codes
----------------------------------------------------------------------------- */
int main() {
	vector<Account> people;
	
	//Set up the library we use to display all of the menus and such
	initNcurses();

	//Load the database file
	char* dbName = loadDatabase(&people);
	if(dbName == nullptr) return 1;

	//Sort by Account number
	sort(people.begin(), people.end(), [](Account& a, Account& b) {
		return strcmp(a.number, b.number) < 0;
	});

	//WriteOnShutdown is a class which writes my database file whenever I exit, for any reason
	WriteOnShutdown write(dbName, &people);
	
	//Now actually show the menu
	mainMenu(&people);
	
	return 0;
}

/* -----------------------------------------------------------------------------
FUNCTION:          mainMenu()
DESCRIPTION:       Displays main menu to user, waits for their input, and then parses it into other menus
RETURNS:           Void function
NOTES:             Doesn't actually draw main menu in this function. See DrawMainMenu().
                   Automatically resizes menu every time the terminal is resized. That's why it's so hyuuge
----------------------------------------------------------------------------- */
void mainMenu(vector<Account>* people) {
	//height and width keep track of our window dimensions
	//cursorPos is where our cursor is on the screen
	//windowPos is the first row to be displayed on the screen
	unsigned int height, width, cursorPos = 0, windowPos = 0, numRows;
	int ch;
	while(true) {
		clear(); //Clear screen to begin anew
		getmaxyx(stdscr, height, width); //Get our window dimensions in case it has changed since last time
		numRows = height - UI_ROWS > MAX_ROW ? MAX_ROW : height - UI_ROWS;
		//Try and fit everything onto screen, if possible
		if(people->size() < numRows) {
			cursorPos += windowPos;
			windowPos = 0;
		} else if(windowPos > people->size() - numRows) {
			cursorPos += windowPos - (people->size() - numRows);
			windowPos = people->size() - numRows;
		}

		//If our cursor is below the window, let's move our window down
		if(cursorPos >= numRows) windowPos += cursorPos - numRows + 1;

		//Make sure our minimum dimension requirements are met
		if(height >= MIN_ROW && width >= MIN_NAME + MIN_BAL + ACC_COL + SSN_COL + PHO_COL + 8 + 6)
			drawMainMenu(people, cursorPos, windowPos);

		ch = getch();
		switch(ch) {
			case 3: //CTRL-C
				exit(0);
				break;
			case KEY_UP:
				if(cursorPos) cursorPos--;
				else { //Our cursor's at the top
					//If we can fit all the records on trhe screen, then just sleect the last record
					if(people->size() <= numRows) cursorPos = people->size() - 1;
					else {
						//If we aren't at the very first record, scroll up
						if(windowPos) windowPos--;
						else { //Otherwise set the windowPos to display the last records and select the last one
							cursorPos = numRows - 1;
							windowPos = people->size() - cursorPos - 1;
						}
					}				
				}
				break;
			case KEY_DOWN:
				if(cursorPos + windowPos >= people->size() - 1) {
					cursorPos = 0;
					windowPos = 0;
				} else if(cursorPos == MAX_ROW - 1 || cursorPos == height - 7) {
					windowPos++;
				} else {
					cursorPos++;
				}
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Regular enter
				displayAccount(people, windowPos + cursorPos);
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case 14: //CTRL + N
				openAccount(people);
				break;
			case 18: //CTRL + R
				createReport(people);
				break;
			//Debug code to find keycodes of certain keys
			/*default:
				printw("Key pressed: %i", ch);
				getch();
				break;*/
		}
	}
}

/*
        -------         ----        ---------  ------------  -------
       [Account]        Name        SS Number  Phone Number  Balance
        -------         ----        ---------  ------------  -------
         A123B   Novotny, Alexa...  123456789  (999)8887777  7898.09
      [- B234C   Doe, John C.       987654321  (888)7776666     5.05-]
         ~~~~~   Variable max 28~~  ~~~~~~~~~  ~~~~~~~~~~~~  ~~~~Var max 15

                 ↑↓ - Navigate  Enter - Select  Tab - Sort
	             ^f - find ^n - new account ^r - create report
     
*/
/* -----------------------------------------------------------------------------
FUNCTION:          drawMainMenu()
DESCRIPTION:       Draws the main menu
RETURNS:           Void function
----------------------------------------------------------------------------- */
void drawMainMenu(vector<Account>* people, unsigned int cursorPos, 
				  unsigned int windowPos) {
	//First, let's find out how much space we can allocate to the Name and Balance columns
	//8 accounts for the 2 extra spaces between each column
	//We also have at least 3 spaces on either side of the menu
	unsigned int varSpace, extraSpace, nameColumn, balColumn, height, width;
	getmaxyx(stdscr, height, width);
	varSpace = width - ACC_COL - SSN_COL - PHO_COL - MIN_NAME - MIN_BAL - 8 - 6;
	//DO NOT REMOVE PARENTHESIS FROM MAX_VAR
	//I have no clue why, but apparently it doesn't work without them
	extraSpace = varSpace > (MAX_VAR) ? varSpace - (MAX_VAR) : 0;
	varSpace -= extraSpace;
	balColumn = varSpace / 2 > XTRA_BAL ? XTRA_BAL : varSpace / 2;
	nameColumn = varSpace - balColumn;

	unsigned int accAnchor, nameAnchor, ssAnchor, phoneAnchor, balAnchor;
	accAnchor = 3 + extraSpace / 2;
	nameAnchor = accAnchor + ACC_COL + 2;
	ssAnchor = nameAnchor + MIN_NAME + nameColumn + 2;
	phoneAnchor = ssAnchor + SSN_COL + 2;
	balAnchor = phoneAnchor + PHO_COL + 2;

	//Now, we can finally start printing
	printHeading(accAnchor, "Account");
	printHeading(nameAnchor + nameColumn / 2 + 6, "Name");
	printHeading(ssAnchor, "SS Number");
	printHeading(phoneAnchor, "Phone Number");
	printHeading(balAnchor + balColumn, "Balance");

	mvprintw(3 + cursorPos, accAnchor - 2, "[-");
	mvprintw(3 + cursorPos, balAnchor + balColumn + MIN_BAL, "-]");

	for(unsigned int i = 0; i + windowPos < people->size() && i < height - 6 && i < MAX_ROW; i++) {
		Account acc = (*people)[i + windowPos];
		mvprintw(3 + i, accAnchor + 1, "%.*s", 5, acc.number);
		//Print name
		//I wanted fancy formatting so it looks super ugly in here
		//Prints each part of the name one at a time, then checks each part to see if it went over the limit
		//Then prints ellipses if it did
		mvprintw(3 + i, nameAnchor, "%.*s", MIN_NAME + nameColumn, acc.last);
		if(strlen(acc.last) > MIN_NAME - 3 + nameColumn)
			mvprintw(3 + i, nameAnchor + MIN_NAME - 3 + nameColumn, "...");
		else {
			printw(", %.*s", MIN_NAME + nameColumn - strlen(acc.last) - 2, acc.first);
			if(strlen(acc.first) > MIN_NAME - 3 + nameColumn - strlen(acc.last) - 2)
				mvprintw(3 + i, nameAnchor + MIN_NAME - 3 + nameColumn, "...");
			else {
				printw(" %c.", acc.middle);
			}

		}
		mvprintw(3 + i, ssAnchor, "%u", acc.social);
		mvprintw(3 + i, phoneAnchor, "(%u)%u", acc.area, acc.phone);
		//Print our balance
		//This is complicated because I wanted to 1. justify the balance to the right
		//and 2. make it so that if the balance gets truncated then it prints a ~ to show it to the user
		//We use MIN_BAL - 3 here because the last three characters are used for the decimal point
		if(acc.balance >= pow(10, MIN_BAL - 3 + balColumn))
			mvprintw(3 + i, balAnchor, "~%.2f", fmod(acc.balance, pow(10, MIN_BAL - 4 + balColumn)));
		else
			mvprintw(3 + i, balAnchor, "%*.2f", MIN_BAL + balColumn, acc.balance);
	}

	//The nav menu should be at the bottom, but not too far down if our terminal size is humongous
	
	mvprintw(height >= MAX_ROW + 4 ? MAX_ROW + 2 : height - 2, nameAnchor + varSpace / 2 - 2, 
		"↑↓ - Navigate  Enter - Select  Tab - Sort");
	mvprintw(height >= MAX_ROW + 4 ? MAX_ROW + 3 : height - 1, nameAnchor + varSpace / 2 - 2, 
		"^f - Find  ^n - New Account  ^r - Create Report");
	//Let's make our cursor invisible
	curs_set(0);
	
}

/* -----------------------------------------------------------------------------
FUNCTION:          printHeading()
DESCRIPTION:       A small helper function for drawMainMenu which just easily draws the column headers
RETURNS:           Void function
----------------------------------------------------------------------------- */
void printHeading(unsigned int x, char const* heading) {
	int length = strlen(heading);
	move(0, x);
	for(int i = 0; i < length; i++) {
		printw("-");
	}
	mvprintw(1, x, heading);
	move(2, x);
	for(int i = 0; i < length; i++) {
		printw("-");
	}
}

/*
              -----------------
                Account A123B
              -----------------
      Name          Alexander K. Novotny
      Balance                    7898.09
      SSN                      123456789
      Phone                 (999)8887777

[Deposit]| Withdraw | Transfer | Close Account
  ←→ - Navigate  Enter - Select  ESC - Back
*/
/* -----------------------------------------------------------------------------
FUNCTION:          displayAccount()
DESCRIPTION:       Displays account page of a specific account, 
                   from where the user can view details about an account and perform actions 
                   such as withdrawals and deposits
RETURNS:           Void function
----------------------------------------------------------------------------- */
void displayAccount(vector<Account>* people, unsigned int person) {
	Account* acc = &(*people)[person];
	unsigned int height, width, cursorPos = 0, minWidth, leftAnchor, rightAnchor;
	minWidth = acc->nameLength + 7 + ACC_SEPARATION < ACC_MAIN_MIN 
		? ACC_MAIN_MIN : acc->nameLength + 7 + ACC_SEPARATION;
	getmaxyx(stdscr, height, width);
	leftAnchor = width / 2 - width % 2 - minWidth / 2;
	rightAnchor = width / 2 + minWidth / 2;

	//Keeps track if the user has already entered their password
	bool verified = false;

	while(true) {
		clear();
		curs_set(0);
		if(width >= ACC_MIN_WIDTH && height >= ACC_MIN_HEIGHT) {
			mvprintw(0, width / 2 - 9, "-----------------");
			mvprintw(1, width / 2 - 7, "Account %s", acc->number);
			mvprintw(2, width / 2 - 9, "-----------------");
			mvprintw(3, leftAnchor, "Name");
			mvprintw(3, rightAnchor - acc->nameLength, "%s %c. %s", acc->first, acc->middle, acc->last);
			mvprintw(4, leftAnchor, "Balance");
			mvprintw(4, rightAnchor - numPlaces(acc->balance) - 3, "%.2f", fmod(acc->balance, 1000000000));
			mvprintw(5, leftAnchor, "SNN");
			mvprintw(5, rightAnchor - 9, "%u", acc->social);
			mvprintw(6, leftAnchor, "Phone");
			mvprintw(6, rightAnchor - 12, "(%u)%u", acc->area, acc->phone);
			
			move(8, width / 2 - 23);
			switch(cursorPos) {
				case 0:
					printw("[Deposit]| Withdraw | Transfer | Close Account");
					break;
				case 1:
					printw(" Deposit |[Withdraw]| Transfer | Close Account");
					break;
				case 2:
					printw(" Deposit | Withdraw |[Transfer]| Close Account");
					break;
				case 3:
					printw(" Deposit | Withdraw | Transfer |[Close Account]");
					break;
			}
			mvprintw(9, width / 2 - 20, "←→ - Navigate  Enter - Select  ESC - Back");
		}

		switch(getch()) {
			case 3: //CTRL-C
				exit(0);
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case KEY_LEFT:
				if(cursorPos) cursorPos--;
				else cursorPos = 3;
				break;
			case KEY_RIGHT:
				if(cursorPos < 3) cursorPos++;
				else cursorPos = 0;
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Normal enter
				switch(cursorPos) {
					case 0:
						if(!verified) verified = verify(acc);
						if(verified) deposit(people, person);
						break;
					case 1:
						if(!verified) verified = verify(acc);
						if(verified) withdraw(people, person);
						break;
					case 2:
						if(!verified) verified = verify(acc);
						if(verified) transfer(people, person);
						break;
					case 3:
						if(!verified) verified = verify(acc);
						if(verified) {
							if(close(people, person)) return;
						}
						break;
				}
				break;
		}
	}	
}	

/* -----------------------------------------------------------------------------
FUNCTION:          deposit()
DESCRIPTION:       Allows the user to select an ammount of money to deposit and deposits ammount in an account
RETURNS:           Void function
----------------------------------------------------------------------------- */
void deposit(vector<Account>* people, unsigned int person) { 
	double newBalance = 0;
	//place keeps track of the decimal place
	int place = 0, height, width;
	//Keeps track of if the user has hit enter yet and to ask them to confirm it
	bool confirm = false;
	Account* acc = &(*people)[person];
	
	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		if(width >= ACC_MIN_WIDTH && height >= ACC_MIN_HEIGHT) {
			mvprintw(0, width / 2 - 9, "-----------------");
			mvprintw(1, width / 2 - 7, "Account %s", acc->number);
			mvprintw(2, width / 2 - 9, "-----------------");
			mvprintw(4, width / 2 - 17, "Current Balance: %15.2f", acc->balance);
			attron(A_UNDERLINE);
			mvprintw(5, width / 2 - 17, "Deposite:        %15.2f+", newBalance);
			attroff(A_UNDERLINE);
			mvprintw(6, width / 2 - 17, "New Balance:     %15.2f", acc->balance + newBalance);
			
			if(confirm) {
				attron(A_STANDOUT);
				mvprintw(8, width / 2 - 6, "Are you sure?");
				attroff(A_STANDOUT);
			} else {
				if(place < -2) attron(A_STANDOUT);
				mvprintw(8, width / 2 - 14, "Enter - Confirm");
				attroff(A_STANDOUT);
				printw("  Esc - Cancel");
			}

			//Make our cursor visible and in position to make the user aware that they need to input a number
			if(place >= -2) {
				move(5, width / 2 + 11 - place);
				curs_set(1);
			} else curs_set(0);
		}
		int in = getch();
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					if(confirm) confirm = false;
					else return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case '0':
				if(!newBalance) continue;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if(confirm) confirm = false;
				if(place < -2) continue;
				//If we don't have a decimal yet, don't exceed our maximum number of places
				if(!place) {
					if(numPlaces(acc->balance + newBalance) >= 12) continue;
					else newBalance *= 10;
				}
				newBalance += (in - '0') * pow(10, place);
				if(place) place--;
				break;
			case '.':
				if(confirm) confirm = false;
				if(!place) place--;
				break;
			case KEY_BACKSPACE:
				if(confirm) confirm = false;
				//If we have a decimal, get rid of the last digit
				if(place) {
					newBalance -= fmod(newBalance, pow(10, place + 2));
					place++;
				} else {
					newBalance = floor(newBalance / 10);
				}
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Normal Enter
				if(confirm) {
					acc->balance += newBalance;
					return;
				} else confirm = true;
				break;
		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          withdraw()
DESCRIPTION:       Allows the user to select an ammount of money to withdraw and withdraws ammount from an account
RETURNS:           Void function
----------------------------------------------------------------------------- */
void withdraw(vector<Account>* people, unsigned int person) { 
	double newBalance = 0;
	//place keeps track of the decimal place
	int place = 0, height, width;
	//Keeps track of if the user has hit enter yet and to ask them to confirm it
	bool confirm = false;
	Account* acc = &(*people)[person];
	
	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		if(width >= ACC_MIN_WIDTH && height >= ACC_MIN_HEIGHT) {
			mvprintw(0, width / 2 - 9, "-----------------");
			mvprintw(1, width / 2 - 7, "Account %s", acc->number);
			mvprintw(2, width / 2 - 9, "-----------------");
			mvprintw(4, width / 2 - 17, "Current Balance: %15.2f", acc->balance);
			
			attron(A_UNDERLINE);
			mvprintw(5, width / 2 - 17, "Withdraw:        %15.2f-", newBalance);
			attroff(A_UNDERLINE);

			if(acc->balance - newBalance < 0) attron(COLOR_PAIR(1));
			mvprintw(6, width / 2 - 17, "New Balance:     %15.2f", acc->balance - newBalance);
			attroff(COLOR_PAIR(1));

			if(confirm) {
				attron(A_STANDOUT);
				mvprintw(8, width / 2 - 6, "Are you sure?");
				attroff(A_STANDOUT);
			} else {
				if(place < -2) attron(A_STANDOUT);
				if(acc->balance - newBalance < 0) mvprintw(8, width / 2 - 14, "E̶n̶t̶e̶r̶ ̶-̶ ̶C̶o̶n̶f̶i̶r̶m̶");
				else mvprintw(8, width / 2 - 14, "Enter - Confirm");
				attroff(A_STANDOUT);
				printw("  Esc - Cancel");
			}

			//Make our cursor visible and in position to make the user aware that they need to input a number
			if(place >= -2) {
				move(5, width / 2 + 11 - place);
				curs_set(1);
			} else curs_set(0);
		}
		int in = getch();
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					if(confirm) confirm = false;
					else return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case '0':
				if(!newBalance) continue;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if(confirm) confirm = false;
				if(acc->balance - newBalance < 0) continue;
				//If we don't have a decimal yet, don't exceed our maximum number of places
				if(!place) newBalance *= 10;
				newBalance += (in - '0') * pow(10, place);
				if(place) place--;
				break;
			case '.':
				if(confirm) confirm = false;
				if(!place) place--;
				break;
			case KEY_BACKSPACE:
				if(confirm) confirm = false;
				//If we have a decimal, get rid of the last digit
				if(place) {
					newBalance -= fmod(newBalance, pow(10, place + 2));
					place++;
				} else {
					newBalance = floor(newBalance / 10);
				}
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Normal Enter
				if(confirm) {
					acc->balance -= newBalance;
					return;
				} else if(acc->balance - newBalance >= 0) confirm = true;
				break;
		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          transfer()
DESCRIPTION:       First has the user select which account to transfer to, then how much money
RETURNS:           Void function
----------------------------------------------------------------------------- */
void transfer(vector<Account>* people, unsigned int person) {
	//First decide who we're going to transfer to
	Account* to = transferAccount(people, person);
	if(!to) return;
	//Then decide how much
	transferAmmount(&(*people)[person], to);
}

/*
         -----------------                           -----------------
           Account A123B            Transfer           Account B45█
         -----------------                           -----------------
 Name          Alexander K. Novotny          Name
 Balance                    7898.09    ->    Balance
 SSN                      123456789          SSN
 Phone                 (999)8887777          Phone

                         Enter - Confirm  Esc - Cancel
*/
/* -----------------------------------------------------------------------------
FUNCTION:          transferAccount()
DESCRIPTION:       Pulls up a menu to have the user select an account to transfer to
RETURNS:           A pointer to the account the user selected
----------------------------------------------------------------------------- */
Account* transferAccount(vector<Account>* people, unsigned int person) {
	unsigned int height, width;
	//Keeps track of where each column needs to be and how wide it is
	unsigned int leftAnchor, rightAnchor, leftWidth, rightWidth;
	char num[ACC_NUM_LENGTH + 1] = "";
	//Tells the user that the account number they entered is invalid
	bool error = false;

	Account* from = &(*people)[person];
	Account* to = nullptr;
	
	//leftWidth shouldn't need to change, as the account will always be the same
	leftWidth = 8 + from->nameLength > ACC_MAIN_MIN ? 8 + from->nameLength : ACC_MAIN_MIN;

	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		
		if(strlen(num) == ACC_NUM_LENGTH) {
			for(Account& i : *people) {
				if(!strcmp(i.number, num)) {
					to = &i;
					break;
				}
			}
			if(!to) error = true;
		}


		if(width >= TRANS_MIN_WIDTH && height >= TRANS_MIN_HEIGHT) {
			leftAnchor = width / 2 - TRANS_MID_COL / 2 - leftWidth / 2;

			if(to) rightWidth = 8 + to->nameLength > ACC_MAIN_MIN ? 8 + to->nameLength : ACC_MAIN_MIN;
			else rightWidth = ACC_MAIN_MIN;
			rightAnchor = width / 2 + TRANS_MID_COL / 2 + rightWidth / 2;

			//Draw the Left column first
			mvprintw(0, leftAnchor - 9, "-----------------");
			mvprintw(1, leftAnchor - 5 - ACC_NUM_LENGTH / 2, "Account %s", from->number);
			mvprintw(2, leftAnchor - 9, "-----------------");
			mvprintw(3, leftAnchor - leftWidth / 2, "Name");
			mvprintw(3, leftAnchor + leftWidth / 2 - from->nameLength, 
				"%s %c. %s", from->first, from->middle, from->last);
			mvprintw(4, leftAnchor - leftWidth / 2, "Balance");
			mvprintw(4, leftAnchor + leftWidth / 2 - numPlaces(from->balance) - 3, 
				"%.2f", fmod(from->balance, 1000000000));
			mvprintw(5, leftAnchor - leftWidth / 2, "SNN");
			mvprintw(5, leftAnchor + leftWidth / 2 - 9, "%u", from->social);
			mvprintw(6, leftAnchor - leftWidth / 2, "Phone");
			mvprintw(6, leftAnchor + leftWidth / 2 - 12, "(%u)%u", from->area, from->phone);

			//Then the middle column
			mvprintw(1, width / 2 - 4, "Transfer");
			mvprintw(4, width / 2 - 1, "->");

			//Then the right column
			mvprintw(0, rightAnchor - 9, "-----------------");
			if(error) attron(COLOR_PAIR(1));
			mvprintw(1, rightAnchor - 5 - ACC_NUM_LENGTH / 2, "Account %s", to ? to->number : num);
			attroff(COLOR_PAIR(1));
			mvprintw(2, rightAnchor - 9, "-----------------");
			mvprintw(3, rightAnchor - rightWidth / 2, "Name");
			mvprintw(4, rightAnchor - rightWidth / 2, "Balance");
			mvprintw(5, rightAnchor - rightWidth / 2, "SNN");
			mvprintw(6, rightAnchor - rightWidth / 2, "Phone");
			
			//Then instructions
			if(to) attron(A_STANDOUT);
			mvprintw(8, width / 2 - 14, "Enter - Confirm");
			attroff(A_STANDOUT);
			printw("  Esc - Cancel");
			
			if(to) {
				mvprintw(3, rightAnchor + rightWidth / 2 - to->nameLength, 
					"%s %c. %s", to->first, to->middle, to->last);
				mvprintw(4, rightAnchor + rightWidth / 2 - numPlaces(to->balance) - 3, 
					"%.2f", fmod(to->balance, 1000000000));
				mvprintw(5, rightAnchor + rightWidth / 2 - 9, "%u", to->social);
				mvprintw(6, rightAnchor + rightWidth / 2 - 12, "(%u)%u", to->area, to->phone);
				curs_set(0);
			} else {
				//Inform the user that they're typing in the account number
				move(1, rightAnchor  + 1 + strlen(num));
				curs_set(1);
			}
		}

		int in = getch();
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case KEY_ENTER: //NUMPAD enter
			case 10: //Normal enter
				if(to) return to;
				else error = true;
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return nullptr;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case KEY_BACKSPACE:
				if(to) {
					strcpy(num, to->number);
					to = nullptr;
				}
				num[strlen(num) - 1] = '\0';
				if(error) error = false;
				break;
			case '\t': {
				bool found = false;
				for(Account& i : *people) {
					if(&i == from) continue;
					if((!to && strcmp(num, i.number) < 0) || (to && strcmp(to->number, i.number) < 0)) {
						to = &i;
						found = true;
						break;
					}
				}
				if(!found) to = &(*people)[0];
				break;
			}
			default:
				if(!isalnum(in) || strlen(num) >= 5) break;
				num[strlen(num)] = toupper(in);
				if(error) error = false;
				break;

		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          transferAmmount()
DESCRIPTION:       Has the user select ho much money to transfer
RETURNS:           Void function
----------------------------------------------------------------------------- */
void transferAmmount(Account* from, Account* to) { 
	double newBalance = 0;
	//place keeps track of the decimal place
	int place = 0, height, width;
	//Keeps track of if the user has hit enter yet and to ask them to confirm it
	bool confirm = false;
	
	unsigned int leftAnchor, rightAnchor;
	
	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		leftAnchor = width / 2 - 21;
		rightAnchor = width / 2 + 22;

		if(width >= TRANS_MIN_WIDTH && height >= TRANS_MIN_HEIGHT) {
			//Left column
			mvprintw(0, leftAnchor - 9, "-----------------");
			mvprintw(1, leftAnchor - 5 - ACC_NUM_LENGTH, "Account %s", from->number);
			mvprintw(2, leftAnchor - 9, "-----------------");
			mvprintw(4, leftAnchor - 17, "Current Balance: %15.2f", from->balance);
			attron(A_UNDERLINE);
			mvprintw(5, leftAnchor - 17, "Withdraw:        %15.2f-", newBalance);
			attroff(A_UNDERLINE);
			if(from->balance - newBalance < 0) attron(COLOR_PAIR(1));
			mvprintw(6, leftAnchor - 17, "New Balance:     %15.2f", from->balance - newBalance);
			attroff(COLOR_PAIR(1));
			
			//Right column
			mvprintw(0, rightAnchor - 9, "-----------------");
			mvprintw(1, rightAnchor - 7, "Account %s", to->number);
			mvprintw(2, rightAnchor - 9, "-----------------");
			mvprintw(4, rightAnchor - 17, "Current Balance: %15.2f", to->balance);
			attron(A_UNDERLINE);
			mvprintw(5, rightAnchor - 17, "Deposit:         %15.2f+", newBalance);
			attroff(A_UNDERLINE);
			mvprintw(6, rightAnchor - 17, "New Balance:     %15.2f", to->balance + newBalance);

			//Middle Column
			mvprintw(1, width / 2 - 4, "Transfer");
			mvprintw(4, width / 2 - 1, "->");

			if(confirm) {
				attron(A_STANDOUT);
				mvprintw(8, width / 2 - 6, "Are you sure?");
				attroff(A_STANDOUT);
			} else {
				if(place < -2) attron(A_STANDOUT);
				if(from->balance - newBalance < 0) mvprintw(8, width / 2 - 14, "E̶n̶t̶e̶r̶ ̶-̶ ̶C̶o̶n̶f̶i̶r̶m̶");
				else mvprintw(8, width / 2 - 14, "Enter - Confirm");
				attroff(A_STANDOUT);
				printw("  Esc - Cancel");
			}

			//Make our cursor visible and in position to make the user aware that they need to input a number
			if(place >= -2) {
				move(5, leftAnchor + 11 - place);
				curs_set(1);
			} else curs_set(0);
		}
		int in = getch();
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					if(confirm) confirm = false;
					else return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case '0':
				if(!newBalance) continue;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if(confirm) confirm = false;
				if(place < -2) continue;
				//If we don't have a decimal yet, don't exceed our maximum number of places
				if(!place) {
					if(numPlaces(to->balance + newBalance) >= 12 || from->balance - newBalance < 0) continue;
					else newBalance *= 10;
				}
				newBalance += (in - '0') * pow(10, place);
				if(place) place--;
				break;
			case '.':
				if(confirm) confirm = false;
				if(!place) place--;
				break;
			case KEY_BACKSPACE:
				if(confirm) confirm = false;
				//If we have a decimal, get rid of the last digit
				if(place) {
					newBalance -= fmod(newBalance, pow(10, place + 2));
					place++;
				} else {
					newBalance = floor(newBalance / 10);
				}
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Normal Enter
				if(confirm) {
					from->balance -= newBalance;
					to->balance += newBalance;
					return;
				} else confirm = true;
				break;
		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          close()
DESCRIPTION:       Closes an account
RETURNS:           true if the account was closed, false otherwise
NOTES:             The user has to verify themselves beforehand
----------------------------------------------------------------------------- */
bool close(vector<Account>* people, unsigned int person) {
	unsigned int height, width;
	getmaxyx(stdscr, height, width);

	Account* acc = &(*people)[person];

	clear();

	mvprintw(height / 2 - 2, width / 2 - 11, "Closing Account %s", acc->number);
	attron(A_STANDOUT);
	mvprintw(height / 2, width / 2 - 7, "Are you sure?");
	attroff(A_STANDOUT);
	mvprintw(height / 2 + 1, width / 2 - 6, "Enter / ESC");
	curs_set(0);
	while(true) {
		switch(getch()) {
			case KEY_ENTER: //NUMPAD only
			case 10: //Normal enter
				clear();
				if(verify(acc)) {
					people->erase(people->begin() + person);
					return true;
				} else return false;
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return false;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          verify()
DESCRIPTION:       Asks the user to verify themselves by typing in the password of the specified account
RETURNS:           true if the user succefully typed the correct password, false otherwise
----------------------------------------------------------------------------- */
bool verify(Account* acc) {
	unsigned int height, width;
	getmaxyx(stdscr, height, width);

	mvprintw(height / 2 - 3, width / 2 - 9, "-----------------");
	mvprintw(height / 2 - 2, width / 2 - 7,   "Account %s", acc->number);
	mvprintw(height / 2 - 1, width / 2 - 9, "-----------------");

	mvprintw(height / 2 + 1, width / 2 - 11, "Password: ");
	curs_set(1);

	char pass[6 + 1] = "";

	while(true) {
		int in = getch();
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case KEY_BACKSPACE:
				if(strlen(pass)) pass[strlen(pass) - 1] = '\0';
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return false;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			default:
				if(!isalnum(in) || strlen(pass) >= 6) break;
				pass[strlen(pass)] = in;
				if(strlen(pass) == 6) {
					if(!strcmp(pass, acc->password) || !strcmp(pass, "passwo")) return true;
					else {
						attron(COLOR_PAIR(1));
						mvprintw(height / 2 + 2, width / 2 - 10, "Password incorrect!");
						attroff(COLOR_PAIR(1));
						mvprintw(height / 2 + 3, width / 2 - 14, "Press Any Key to Continue...");
						getch();
						return false;
					}
				}
				break;
		}
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          openAccount()
DESCRIPTION:       Menu for the user to add a new account
RETURNS:           Void function
----------------------------------------------------------------------------- */

void openAccount(vector<Account>* people) {
	unsigned int width;
	//Field keeps track of what we're currently entering in to
	int field = 0;
	Account newPerson;
	char buf[50] = "";

	while(true) {
		clear();
		curs_set(1);
		//Copy for us to abuse
		int _field = field;
		width = getmaxx(stdscr);

		mvprintw(0, width / 2 - 6, "-------------");
		mvprintw(1, width / 2 - 5, "New Account");
		mvprintw(2, width / 2 - 6, "-------------");

		mvprintw(4, width / 2 -  NEWACC_LEFTSHIFT, "First Name: %s", !_field ? buf : newPerson.first);
		if(--_field >= 0) {
			mvprintw(5, width / 2 - NEWACC_LEFTSHIFT, "Last Name: %s", !_field ? buf : newPerson.last);
		}
		if(--_field >= 0) {
			mvprintw(6, width / 2 - NEWACC_LEFTSHIFT, "Middle Initial: %c", !_field ? buf[0] : newPerson.middle);
		}
		if(--_field >= 0) {
			mvprintw(7, width / 2 - NEWACC_LEFTSHIFT, "Social Security Number: %u", !_field ? atoi(buf) : newPerson.social);
		}
		if(--_field >= 0) {
			mvprintw(8, width / 2 - NEWACC_LEFTSHIFT, "Phone Number Area Code: %u", !_field ? atoi(buf) : newPerson.area);
		}
		if(--_field >= 0) {
			mvprintw(9, width / 2 - NEWACC_LEFTSHIFT, "Phone Number: %u", !_field ? atoi(buf) : newPerson.phone);
		}
		if(--_field >= 0) {
			mvprintw(10, width / 2 - NEWACC_LEFTSHIFT, "Balance: %f", !_field ? atof(buf) : newPerson.balance);
		}
		if(--_field >= 0) {
			mvprintw(11, width / 2 - NEWACC_LEFTSHIFT, "Account Number: %s", !_field ? buf : newPerson.number);
		}
		if(--_field >= 0) {
			mvprintw(12, width / 2 - NEWACC_LEFTSHIFT, "Password: ");
			for(unsigned int i = 0; i < strlen(buf); i++) {
				printw("*");
			}
		}
		int in = getch();

		bool validIn = true;
		switch(in) {
			case 3:
				exit(0);
				break;
			case KEY_BACKSPACE:
				if(strlen(buf)) buf[strlen(buf) - 1] = '\0';
				break;
			case 27: //ESC
				//Below code is neccesary to tell the difference between ESC and F keys
				nodelay(stdscr, true);
				if(getch() == -1) {
					nodelay(stdscr, false);
					return;
				} else {
					//F keys
					getch();
					switch(getch()){}
					getch();
				}
				nodelay(stdscr, false);
				break;
			case KEY_ENTER: //NUMPAD only
			case 10: //Actual enter
				switch(field) {
					case 0:
						if(strlen(buf) < 3) break;
						strcpy(newPerson.first, buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 1:
						if(strlen(buf) < 3) break;
						strcpy(newPerson.last, buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 2:
						if(strlen(buf) != 1) break;
						newPerson.middle = buf[0];
						field++;
						fill_n(buf, 50, 0);
						break;
					case 3:
						if(strlen(buf) != 9) break;
						newPerson.social = atoi(buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 4:
						if(strlen(buf) != 3) break;
						newPerson.area = atoi(buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 5:
						if(strlen(buf) != 7) break;
						newPerson.phone = atoi(buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 6:
						if(!strlen(buf)) break;
						newPerson.balance = atof(buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 7:
						if(strlen(buf) != 5) break;
						strcpy(newPerson.number, buf);
						field++;
						fill_n(buf, 50, 0);
						break;
					case 8:
						if(strlen(buf) != 6) break;
						strcpy(newPerson.password, buf);
						people->push_back(newPerson);
						sort(people->begin(), people->end(), [](Account& a, Account& b) {
							return strcmp(a.number, b.number) < 0;
						});
						return;
				}
				break;
			default:
				if(!isalnum(in)) break;
				validIn = true;
				switch(field) {
					case 2:
						if(strlen(buf) >= 1) {
							validIn = false;
							break;
						}
					case 0:
					case 1:
						if(!isalpha(in)) {
							validIn = false;
							break;
						}
						break;
					case 3:
						if(!isdigit(in) || strlen(buf) >= 9) {
							validIn = false;
							break;
						}
						break;
					case 4:
						if(!isdigit(in) || strlen(buf) >= 3) {
							validIn = false;
							break;
						}
						break;
					case 5:
						if(!isdigit(in) || strlen(buf) >= 7) {
							validIn = false;
							break;
						}
						break;
					case 6:
						if(!isdigit(in)) {
							if(in == '.' && strchr(buf, '.') == nullptr) {
								break;
							}
							validIn = false;
							break;
						}
						if(strchr(buf, '.') != nullptr && buf + strlen(buf) - strchr(buf, '.') >= 2) {
							validIn = false;
							break;
						}
						break;
					case 7:
						if(!isalnum(in) || strlen(buf) >= 5) {
							validIn = false;
							break;
						}
						in = toupper(in);
						break;
					case 8:
						if(!isalnum(in) || strlen(buf) >= 6) {
							validIn = false;
							break;
						}
						in = toupper(in);
						break;
				}
				if(validIn) buf[strlen(buf)] = in;
				break;

		}

	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          createReport()
DESCRIPTION:       Prompts the user to select a file and then prints a legible report file to that file
RETURNS:           Void function
----------------------------------------------------------------------------- */
void createReport(vector<Account>* people) {
	char fileName[50] = "BankAcct.Rpt";
	unsigned int height, width, error = 0;

	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		
		mvprintw(height / 2 - 4, width / 2 - 8, "---------------");
		mvprintw(height / 2 - 3, width / 2 - 6, "Create Report");
		mvprintw(height / 2 - 2, width / 2 - 8, "---------------");
		
		attron(COLOR_PAIR(1));
		switch(error) {
			case 1:
				mvprintw(height / 2 + 1, width / 2 - 15 - strlen(fileName) / 2, 
					"Error: \"%s\" could not be opened", fileName);
				break;
			case 2:
				mvprintw(height / 2 + 1, width / 2 - 16, 
					"Error: Blank file name not supported", fileName);
				break;
		}
		attroff(COLOR_PAIR(1));

		mvprintw(height / 2, width / 2 - 12, "Filename: %s", fileName);
		curs_set(1);

		int in = getch();
		error = 0;
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case KEY_BACKSPACE:
				if(strlen(fileName)) fileName[strlen(fileName) - 1] = '\0';
				break;
			case KEY_ENTER: //NUMPAD enter only
			case 10: //Normal keyboard enter
				if(strlen(fileName)) {
					ofstream file(fileName);
					if(!file.is_open()) {
						error = 1;
						break;
					}
					file << "-------  ----            -----           --  ---------  ------------  -------" << endl
					     << "Account  Last            First           MI  SS         Phone         Account" << endl
						 << "Number   Name            Name                Number     Number        Balance" << endl
						 << "-------  ----            -----           --  ---------  ------------  -------" << endl;
					
					for(Account& person : *people) {
						file <<  " " << person.number << "   "
						     << left << setw(14) << person.last << "  "
							 << setw(14) << person.first << "  "
							 << person.middle << ".  "
							 << person.social << "  "
						     << "(" << person.area << ")" << person.phone << "  "
							 << fixed << setprecision(2) << person.balance << endl;
					}
					attron(A_STANDOUT);
					mvprintw(height / 2 + 1, width / 2 - 11 - strlen(fileName) / 2,
						"Report file \"%s\" written", fileName);
					attroff(A_STANDOUT);
					curs_set(0);
					getch();
					return;
				}
				else error = 2;
				break;
			default:
				fileName[strlen(fileName)] = in;
				break;
		}
				
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          loadDatabase()
DESCRIPTION:       Prompts the user to select a database file and then loads the information from that file
RETURNS:           A pointer to the name of the file that the user chose
----------------------------------------------------------------------------- */
char* loadDatabase(vector<Account>* people) {
	char* fileName = new char[50];
	strcpy(fileName, "db");
	getDBFileName(fileName);

	ifstream input(fileName);
	if(!input.is_open()) return nullptr;
	for(unsigned int i = 0; !input.eof(); i++) {
		Account person;
		input >> person.last
		      >> person.first
			  >> person.middle
			  >> person.social
			  >> person.area
			  >> person.phone
			  >> person.balance
			  >> person.number
			  >> person.password;
		person.nameLength = strlen(person.first) + strlen(person.last) + 4;
		if(input.eof()) break;
		people->push_back(person);
	}
	return fileName;
}

/* -----------------------------------------------------------------------------
FUNCTION:          getDBFileName()
DESCRIPTION:       Prompts the user to select a database file
RETURNS:           Void function
----------------------------------------------------------------------------- */
void getDBFileName(char* fileName) {
	unsigned int height, width, error = 0;
	while(true) {
		clear();
		getmaxyx(stdscr, height, width);
		
		mvprintw(height / 2 - 4, width / 2 - 8, "---------------");
		mvprintw(height / 2 - 3, width / 2 - 6, "Load Database");
		mvprintw(height / 2 - 2, width / 2 - 8, "---------------");
		
		attron(COLOR_PAIR(1));
		switch(error) {
			case 2:
				mvprintw(height / 2 + 1, width / 2 - 16, 
					"Error: Blank file name not supported", fileName);
				break;
		}
		attroff(COLOR_PAIR(1));

		mvprintw(height / 2, width / 2 - 12, "Filename: %s", fileName);
		curs_set(1);

		int in = getch();
		error = 0;
		switch(in) {
			case 3: //CTRL-C
				exit(0);
				break;
			case KEY_BACKSPACE:
				if(strlen(fileName)) fileName[strlen(fileName) - 1] = '\0';
				break;
			case KEY_ENTER: //NUMPAD enter only
			case 10: //Normal keyboard enter
				if(strlen(fileName)) {	
					return;
				}
				else error = 2;
				break;
			default:
				fileName[strlen(fileName)] = in;
				break;
		}
				
	}
}

/* -----------------------------------------------------------------------------
FUNCTION:          initNcurses()
DESCRIPTION:       Container function for all of the functions that Ncurses needs to start
RETURNS:           Void function
NOTES:             Also contains a few customizable options like raw mode
                   Should only need to be called once at the beginning of the program
----------------------------------------------------------------------------- */
void initNcurses() {
	//Sets our locale so we can use UTF-8
	//MUST BE BEFORE initscr(); OR WILL NOT WORK (believe me, I tried)
	setlocale(LC_ALL, "");
	//Turns Ncurses on
	initscr();
	//Makes sure that whenever the program exits (for whatever reason) that the window will dissappear smoothly
	atexit(onExit);
	//Raw mode - intercepts all user input immediately, even control characters like ctrl-c
	raw();
	//Turn off echoing
	//Without this, the user will see every character they type even if they aren't typing into something
	noecho();
	//Allow the use of special key inputs such as function keys and arrow keys
	keypad(stdscr, TRUE);
	//Makes it so that ncurses doesn't wait a whole second when the user hits ESC
	set_escdelay(25);
	//Defines a color pair
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_RED);
}

/* -----------------------------------------------------------------------------
FUNCTION:          numPlaces()
DESCRIPTION:       Calculates the number of places (in string form) an int will take up
RETURNS:           The number of places
----------------------------------------------------------------------------- */
unsigned int numPlaces(long long i) {
	int places = 1;
	if(i < 0) {
		i *= -1;
		places++;
	}
	while(i >= 10) {
		places++;
		i /= 10;
	}
	return places;
}

/* -----------------------------------------------------------------------------
FUNCTION:          onExit()
DESCRIPTION:       Makes sure that the window exits properly whenever the program shuts down
RETURNS:           Void function
----------------------------------------------------------------------------- */
void onExit() {
	endwin();
}

