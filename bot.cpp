/**
 * Tetris2 样例程序
 * 20171031更新：将elimBonus[count]改为elimBonus[count - hasBonus]。
 * 20171027更新：将trans数组的第二维长度从4加大到6，感谢kczno1用户的指正。
 * https://wiki.botzone.org/index.php?title=Tetris2
 */
// 注意：x的范围是1~MAPWIDTH，y的范围是1~MAPHEIGHT
// 数组是先行（y）后列（c）
// 坐标系：原点在左下角

#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
using namespace std;

#define MAPWIDTH 10
#define MAPHEIGHT 20

// 我所在队伍的颜色（0为红，1为蓝，仅表示队伍，不分先后）
int currBotColor;
int enemyColor;

// 先y后x，记录地图状态，0为空，1为以前放置，2为刚刚放置，负数为越界
// （2用于在清行后将最后一步撤销再送给对方）
int gridInfo[2][MAPHEIGHT + 2][MAPWIDTH + 2] = { 0 };

// 代表分别向对方转移的行
int trans[2][6][MAPWIDTH + 2] = { 0 };

// 转移行数
int transCount[2] = { 0 };

// 运行eliminate后的当前高度
int maxHeight[2] = { 0 };

// 总消去行数的分数之和
int elimTotal[2] = { 0 };

// 连续几回合发生过消去了
int elimCombo[2] = { 0 };

// 一次性消去行数对应分数
const int elimBonus[] = { 0, 1, 3, 5, 7 };

// 给对应玩家的各类块的数目总计
int typeCountForColor[2][7] = { 0 };

const int blockShape[7][4][8] = {
	{ { 0,0,1,0,-1,0,-1,-1 },{ 0,0,0,1,0,-1,1,-1 },{ 0,0,-1,0,1,0,1,1 },{ 0,0,0,-1,0,1,-1,1 } },
	{ { 0,0,-1,0,1,0,1,-1 },{ 0,0,0,-1,0,1,1,1 },{ 0,0,1,0,-1,0,-1,1 },{ 0,0,0,1,0,-1,-1,-1 } },
	{ { 0,0,1,0,0,-1,-1,-1 },{ 0,0,0,1,1,0,1,-1 },{ 0,0,-1,0,0,1,1,1 },{ 0,0,0,-1,-1,0,-1,1 } },
	{ { 0,0,-1,0,0,-1,1,-1 },{ 0,0,0,-1,1,0,1,1 },{ 0,0,1,0,0,1,-1,1 },{ 0,0,0,1,-1,0,-1,-1 } },
	{ { 0,0,-1,0,0,1,1,0 },{ 0,0,0,-1,-1,0,0,1 },{ 0,0,1,0,0,-1,-1,0 },{ 0,0,0,1,1,0,0,-1 } },
	{ { 0,0,0,-1,0,1,0,2 },{ 0,0,1,0,-1,0,-2,0 },{ 0,0,0,1,0,-1,0,-2 },{ 0,0,-1,0,1,0,2,0 } },
	{ { 0,0,0,1,-1,0,-1,1 },{ 0,0,-1,0,0,-1,-1,-1 },{ 0,0,0,-1,1,-0,1,-1 },{ 0,0,1,0,0,1,1,1 } }
}; // 7种形状(长L| 短L| 反z| 正z| T| 直一| 田格)，4种朝向(上左下右)，8:每相邻的两个分别为x，y

const int rotateBlank[7][4][10] = {
	{ { 1,1,0,0 },{ -1,1,0,0 },{ -1,-1,0,0 },{ 1,-1,0,0 } },
	{ { -1,-1,0,0 },{ 1,-1,0,0 },{ 1,1,0,0 },{ -1,1,0,0 } },
	{ { 1,1,0,0 },{ -1,1,0,0 },{ -1,-1,0,0 },{ 1,-1,0,0 } },
	{ { -1,-1,0,0 },{ 1,-1,0,0 },{ 1,1,0,0 },{ -1,1,0,0 } },
	{ { -1,-1,-1,1,1,1,0,0 },{ -1,-1,-1,1,1,-1,0,0 },{ -1,-1,1,1,1,-1,0,0 },{ -1,1,1,1,1,-1,0,0 } },
	{ { 1,-1,-1,1,-2,1,-1,2,-2,2 } ,{ 1,1,-1,-1,-2,-1,-1,-2,-2,-2 } ,{ -1,1,1,-1,2,-1,1,-2,2,-2 } ,{ -1,-1,1,1,2,1,1,2,2,2 } },
	{ { 0,0 },{ 0,0 } ,{ 0,0 } ,{ 0,0 } }
}; // 旋转的时候需要为空的块相对于旋转中心的坐标

class Tetris
{
public:
	const int blockType;   // 标记方块类型的序号 0~6
	int blockX;            // 旋转中心的x轴坐标
	int blockY;            // 旋转中心的y轴坐标
	int orientation;       // 标记方块的朝向 0~3
	const int(*shape)[8]; // 当前类型方块的形状定义

	int color;

	Tetris(int t, int color) : blockType(t), shape(blockShape[t]), color(color)
	{ }

	inline Tetris &set(int x = -1, int y = -1, int o = -1)
	{
		blockX = x == -1 ? blockX : x;
		blockY = y == -1 ? blockY : y;
		orientation = o == -1 ? orientation : o;
		return *this;
	}

	// 判断当前位置是否合法
	inline bool isValid(int x = -1, int y = -1, int o = -1)
	{
		x = x == -1 ? blockX : x;
		y = y == -1 ? blockY : y;
		o = o == -1 ? orientation : o;
		if (o < 0 || o > 3)
			return false;

		int i, tmpX, tmpY;
		for (i = 0; i < 4; i++)
		{
			tmpX = x + shape[o][2 * i];
			tmpY = y + shape[o][2 * i + 1];
			if (tmpX < 1 || tmpX > MAPWIDTH ||
				tmpY < 1 || tmpY > MAPHEIGHT ||
				gridInfo[color][tmpY][tmpX] != 0)
				return false;
		}
		return true;
	}

	// 判断是否落地
	inline bool onGround()
	{
		if (isValid() && !isValid(-1, blockY - 1))
			return true;
		return false;
	}

	// 将方块放置在场地上
	inline bool place()
	{
		if (!onGround())
			return false;

		int i, tmpX, tmpY;
		for (i = 0; i < 4; i++)
		{
			tmpX = blockX + shape[orientation][2 * i];
			tmpY = blockY + shape[orientation][2 * i + 1];
			gridInfo[color][tmpY][tmpX] = 2;
		}
		return true;
	}

	// 检查能否逆时针旋转自己到o
	inline bool rotation(int o)
	{
		if (o < 0 || o > 3)
			return false;

		if (orientation == o)
			return true;

		int fromO = orientation;
		int i, blankX, blankY;
		while (true)
		{
			if (!isValid(-1, -1, fromO))
				return false;

			if (fromO == o)
				break;

	        // 检查旋转碰撞
	        for (i = 0; i < 5; i++) {
	            blankX = blockX + rotateBlank[blockType][fromO][2 * i];
	            blankY = blockY + rotateBlank[blockType][fromO][2 * i + 1];
	            if (blankX == blockX && blankY == blockY)
	                break;
	            if (gridInfo[color][blankY][blankX] != 0)
	                return false;
	        }

			fromO = (fromO + 1) % 4;
		}
		return true;
	}
};

// 围一圈护城河
void init()
{
	int i;
	for (i = 0; i < MAPHEIGHT + 2; i++)
	{
		gridInfo[1][i][0] = gridInfo[1][i][MAPWIDTH + 1] = 1;
		gridInfo[0][i][0] = gridInfo[0][i][MAPWIDTH + 1] = 1;
	}
	for (i = 0; i < MAPWIDTH + 2; i++)
	{
		gridInfo[1][0][i] = gridInfo[1][MAPHEIGHT + 1][i] = 1;
		gridInfo[0][0][i] = gridInfo[0][MAPHEIGHT + 1][i] = 1;
	}
}

namespace Util
{

	// 检查能否从场地顶端直接落到当前位置
	inline bool checkDirectDropTo(int color, int blockType, int x, int y, int o)
	{
		auto &def = blockShape[blockType][o];
		for (; y <= MAPHEIGHT; y++)
			for (int i = 0; i < 4; i++)
			{
				int _x = def[i * 2] + x, _y = def[i * 2 + 1] + y;
				if (_y > MAPHEIGHT)
					continue;
				if (_y < 1 || _x < 1 || _x > MAPWIDTH || gridInfo[color][_y][_x])
					return false;
			}
		return true;
	}



    // 消去行
	void eliminate(int color)
	{
		int &count = transCount[color] = 0;
		int i, j, emptyFlag, fullFlag, firstFull = 1, hasBonus = 0;
		maxHeight[color] = MAPHEIGHT;
		for (i = 1; i <= MAPHEIGHT; i++)
		{
			emptyFlag = 1;
			fullFlag = 1;
			for (j = 1; j <= MAPWIDTH; j++)
			{
				if (gridInfo[color][i][j] == 0)
					fullFlag = 0;
				else
					emptyFlag = 0;
			}
			if (fullFlag)
			{
				if (firstFull && ++elimCombo[color] >= 3)
				{
					// 奖励行
					for (j = 1; j <= MAPWIDTH; j++)
						trans[color][count][j] = gridInfo[color][i][j] == 1 ? 1 : 0;
					count++;
					hasBonus = 1;
				}
				firstFull = 0;
				for (j = 1; j <= MAPWIDTH; j++)
				{
					// 注意这里只转移以前的块，不包括最后一次落下的块（“撤销最后一步”）
					trans[color][count][j] = gridInfo[color][i][j] == 1 ? 1 : 0;
					gridInfo[color][i][j] = 0;
				}
				count++;
			}
			else if (emptyFlag)
			{
				maxHeight[color] = i - 1;
				break;
			}
			else
				for (j = 1; j <= MAPWIDTH; j++)
				{
					gridInfo[color][i - count + hasBonus][j] =
						gridInfo[color][i][j] > 0 ? 1 : gridInfo[color][i][j];
					if (count)
						gridInfo[color][i][j] = 0;
				}
		}
		if (count == 0)
			elimCombo[color] = 0;
		maxHeight[color] -= count - hasBonus;
		elimTotal[color] += elimBonus[count - hasBonus];
	}

	// 转移双方消去的行，返回-1表示继续，否则返回输者
	int transfer()
	{
		int color1 = 0, color2 = 1;
		if (transCount[color1] == 0 && transCount[color2] == 0)
			return -1;
		if (transCount[color1] == 0 || transCount[color2] == 0)
		{
			if (transCount[color1] == 0 && transCount[color2] > 0)
				swap(color1, color2);
			int h2;
			maxHeight[color2] = h2 = maxHeight[color2] + transCount[color1];
			if (h2 > MAPHEIGHT)
				return color2;
			int i, j;

			for (i = h2; i > transCount[color1]; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color2][i][j] = gridInfo[color2][i - transCount[color1]][j];

			for (i = transCount[color1]; i > 0; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color2][i][j] = trans[color1][i - 1][j];
			return -1;
		}
		else
		{
			int h1, h2;
			maxHeight[color1] = h1 = maxHeight[color1] + transCount[color2];//从color1处移动count1去color2
			maxHeight[color2] = h2 = maxHeight[color2] + transCount[color1];

			if (h1 > MAPHEIGHT) return color1;
			if (h2 > MAPHEIGHT) return color2;

			int i, j;
			for (i = h2; i > transCount[color1]; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color2][i][j] = gridInfo[color2][i - transCount[color1]][j];

			for (i = transCount[color1]; i > 0; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color2][i][j] = trans[color1][i - 1][j];

			for (i = h1; i > transCount[color2]; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color1][i][j] = gridInfo[color1][i - transCount[color2]][j];

			for (i = transCount[color2]; i > 0; i--)
				for (j = 1; j <= MAPWIDTH; j++)
					gridInfo[color1][i][j] = trans[color2][i - 1][j];

			return -1;
		}
	}

	// 颜色方还能否继续游戏
	inline bool canPut(int color, int blockType)
	{
		Tetris t(blockType, color);
		for (int y = MAPHEIGHT; y >= 1; y--)
			for (int x = 1; x <= MAPWIDTH; x++)
				for (int o = 0; o < 4; o++)
				{
					t.set(x, y, o);
					if (t.isValid() && checkDirectDropTo(color, blockType, x, y, o))
						return true;
				}
		return false;
	}

	// 打印场地用于调试
	inline void printField()
	{
        #ifndef _BOTZONE_ONLINE
        		static const char *i2s[] = {
        			"~~",
        			"~~",
        			"  ",
        			"[]",
        			"##"
        		};
        		cout << "~~：墙，[]：块，##：新块" << endl;
        		for (int y = MAPHEIGHT + 1; y >= 0; y--)
        		{
        			for (int x = 0; x <= MAPWIDTH + 1; x++)
        				cout << i2s[gridInfo[0][y][x] + 2];
        			for (int x = 0; x <= MAPWIDTH + 1; x++)
        				cout << i2s[gridInfo[1][y][x] + 2];
        			cout << endl;
        		}
        #endif
	}
}

int a[105][105]={0};
void search1(int x,int y,int cnt)
{
    if (a[x][y]!=0) return ;
    a[x][y]=cnt;
    if (a[x+1][y]==0) search1(x+1,y,cnt);
    if (a[x][y+1]==0) search1(x,y+1,cnt);
    if (a[x-1][y]==0) search1(x-1,y,cnt);
    if (a[x][y-1]==0) search1(x,y-1,cnt);
    return ;
}

int jin(int testColor)
{
    int i = 0, j = 0, wellDepth = 0, tDepth = 0;
    int totalWellDepth = 0;
    // *) 获取最左边的井数
    wellDepth = 0;
    tDepth = 0;
    // cout<<totalWellDepth<<"----"<<endl;
    // *) 获取中间的井数
    for ( i = 1; i < MAPWIDTH+1 ; i++ )
    {
        tDepth = 0;
        for ( j = 0; j < MAPHEIGHT+2; j++ ) {
            if ( gridInfo[testColor][j][i] == 0 &&
                 gridInfo[testColor][j][i - 1] != 0 &&
                 gridInfo[testColor][j][i + 1] != 0 )
            {
                tDepth++;
            } else {
                wellDepth += tDepth * (tDepth + 1) / 2;
                tDepth = 0;
            }
        }
        wellDepth += tDepth * (tDepth + 1) / 2;
    }
    totalWellDepth += wellDepth;
    //cout<<totalWellDepth<<"----"<<endl;
    //	Util::printField();
    return totalWellDepth;
}


double giveScore2(int x,int y,int o,int testNextType,int testColor)
{
    int testGridInfo[MAPHEIGHT + 2][MAPWIDTH + 2];
    for (int i=0;i<MAPHEIGHT + 2;i++)
        for (int j=0;j<MAPWIDTH + 2;j++)
            testGridInfo[i][j]=gridInfo[testColor][i][j];
    Tetris myBlock1(testNextType, testColor);
    myBlock1.set(x, y, o).place();
    //高度
    double score=0;
    //cout<<MAPHEIGHT-y<<endl;
    int maxY=0,minY=10000;
    for (int i=1;i<8;i+=2)
    {
        maxY=max(maxY,blockShape[testNextType][o][i]);
        minY=min(minY,blockShape[testNextType][o][i]);
    }
    score+=(-45.00158825082766)*(maxY+y-minY);
    // if (MAPHEIGHT-y<0)cout<<MAPHEIGHT-y<<"------------------"<<endl;
    //消行数
    double cnt=0;
    int cnt_2=0;
    int x_t=1;
    for (int i=1;i<MAPHEIGHT+1;i++)
    {
        bool b=1;
        for (int j=1;j<MAPWIDTH + 1;j++)
            if (gridInfo[testColor][i][j]==0) b=0;
        if (b)
            for (int j=1;j<MAPWIDTH+1;j++)
                if (gridInfo[testColor][i][j]==2) x_t++;
        cnt_2+=b;
    }
    cnt=cnt_2;
    score+=(34.181268101392694)*(cnt)*x_t;
    //行变换
    cnt=0;
    for (int i=1;i<MAPHEIGHT+1;i++)
    {
        for (int j=1;j<MAPWIDTH+2;j++)
        {
            if (gridInfo[testColor][i][j]!=gridInfo[testColor][i][j-1]) cnt++;
            // if (gridInfo[testColor][i][j]!=gridInfo[testColor][i][j+1]) cnt++;
        }
    }
    score+=(-32.178882868487753)*cnt;
    //cout<<cnt<<"----"<<endl;
    //列变换
    cnt=0;
    for ( int j = 1; j < MAPWIDTH+1; j++ )
    {
        for ( int i = 1; i < MAPHEIGHT+2; i++ )
        {
            //if (gridInfo[testColor][i][j]!=gridInfo[testColor][i+1][j]) cnt++;
            if (gridInfo[testColor][i][j]!=gridInfo[testColor][i-1][j]) cnt++;
        }
    }
    score+=(-93.48695305445199)*cnt;

    //空洞数
    // for (int i=0;i<MAPHEIGHT + 2;i++)
    //     for (int j=0;j<MAPWIDTH + 2;j++)
    //         a[i][j]=gridInfo[testColor][i][j];
    // cnt=2;
    // for (int i=1;i<MAPHEIGHT + 1;i++)
    //     for (int j=1;j<MAPWIDTH + 1;j++)
    //         if (a[i][j]==0) search1(i,j,cnt),cnt++;
    // cout<<cnt-2<<"-----"<<endl;
    // score+=(-7.899265427351652)*(cnt-3);
    // for (int i=0;i<MAPHEIGHT + 2;i++)
    // {
    //     for (int j=0;j<MAPWIDTH + 2;j++)
    //         cout<<a[i][j]<<' ';
    //     cout<<endl;
    // }
    int totalEmptyHoles = 0;
    for ( int i = 0; i < MAPWIDTH+2; i++ )
    {
        int j = 0;
        int emptyHoles = 0;
        for ( ; j < MAPHEIGHT+2; j++ ) {
            if ( gridInfo[testColor][j][i] != 0 ) {
                j++;
                break;
            }
        }
        for ( ; j < MAPHEIGHT+2; j++ ) {
            if ( gridInfo[testColor][j][i] == 0 ) {
                emptyHoles++;
            }
        }
        totalEmptyHoles += emptyHoles;
    }
    score+=(-78.99265427351652)*(totalEmptyHoles);




    //井字
    cnt=jin(testColor);
    // cnt=0;
    // for (int i=1;i<MAPHEIGHT + 1;i++)
    //     for (int j=1;j<MAPWIDTH + 1;j++)
    //         if (gridInfo[testColor][i][j]==0 &&
    //             gridInfo[testColor][i][j+1]==1 &&
    //             gridInfo[testColor][i][j-1]==1) cnt++;
    score+=(-33.855972247263626)*cnt;
    //cout<<cnt<<"----"<<endl;


    //cout<<score<<endl;
    for (int i=0;i<MAPHEIGHT + 2;i++)
        for (int j=0;j<MAPWIDTH + 2;j++)
            gridInfo[testColor][i][j]=testGridInfo[i][j];
    return score;
}


int giveScore(int x,int y,int o,int testNextType,int testColor)
{
    int testGridInfo[MAPHEIGHT + 2][MAPWIDTH + 2];
    for (int i=0;i<MAPHEIGHT + 2;i++)
        for (int j=0;j<MAPWIDTH + 2;j++)
            testGridInfo[i][j]=gridInfo[testColor][i][j];
    Tetris myBlock1(testNextType, testColor);
    myBlock1.set(x, y, o).place();
    int score=1000000000;
    int cnt=0;
    int scal=100;
    for (int i=1;i<MAPHEIGHT+1;i++)
        for (int j=1;j<MAPWIDTH + 1;j++)
        {
            cnt=0;
            if (gridInfo[testColor][i][j]==0)
            {
                if (gridInfo[testColor][i+1][j]==1) cnt++;
                if (gridInfo[testColor][i][j+1]==1) cnt++;
                if (gridInfo[testColor][i-1][j]==1) cnt++;
                if (gridInfo[testColor][i][j-1]==1) cnt++;
                score-=(1<<(cnt*3))*i;
            }
            else score-=scal*i;
        }
    for (int i=0;i<MAPHEIGHT + 2;i++)
        for (int j=0;j<MAPWIDTH + 2;j++)
            gridInfo[testColor][i][j]=testGridInfo[i][j];
    return score;
}


int main()
{
	// 加速输入
	istream::sync_with_stdio(false);
	srand(time(NULL));
	init();

	int turnID, blockType;
    int blockTypeBegin;
	int nextTypeForColor[2];
	cin >> turnID;

	// 先读入第一回合，得到自己的颜色
	// 双方的第一块肯定是一样的
	cin >> blockType >> currBotColor;
    blockTypeBegin = blockType;
	enemyColor = 1 - currBotColor;
	nextTypeForColor[0] = blockType;
	nextTypeForColor[1] = blockType;
	typeCountForColor[0][blockType]++;
	typeCountForColor[1][blockType]++;

	// 然后分析以前每回合的输入输出，并恢复状态
	// 循环中，color 表示当前这一行是 color 的行为
	// 平台保证所有输入都是合法输入
	for (int i = 1; i < turnID; i++)
	{
		int currTypeForColor[2] = { nextTypeForColor[0], nextTypeForColor[1] };
		int x, y, o;
		// 根据这些输入输出逐渐恢复状态到当前回合

		// 先读自己的输出，也就是自己的行为
		// 自己的输出是自己的最后一步
		// 然后模拟最后一步放置块
		cin >> blockType >> x >> y >> o;

		// 我当时把上一块落到了 x y o！
		Tetris myBlock(currTypeForColor[currBotColor], currBotColor);
		myBlock.set(x, y, o).place();

		// 我给对方什么块来着？
		typeCountForColor[enemyColor][blockType]++;
		nextTypeForColor[enemyColor] = blockType;

		// 然后读自己的输入，也就是对方的行为
		// 裁判给自己的输入是对方的最后一步
		cin >> blockType >> x >> y >> o;

		// 对方当时把上一块落到了 x y o！
		Tetris enemyBlock(currTypeForColor[enemyColor], enemyColor);
		enemyBlock.set(x, y, o).place();

		// 对方给我什么块来着？
		typeCountForColor[currBotColor][blockType]++;
		nextTypeForColor[currBotColor] = blockType;

		// 检查消去
		Util::eliminate(0);
		Util::eliminate(1);

		// 进行转移
		Util::transfer();
	}


	int blockForEnemy, finalX, finalY, finalO;

	// 做出决策（你只需修改以下部分）
//--------------------------------------------------------------------
	// 遇事不决先输出（平台上编译不会输出）
	Util::printField();

	// 贪心决策
	// 从下往上以各种姿态找到第一个位置，要求能够直着落下
	Tetris block(nextTypeForColor[currBotColor], currBotColor);
    double score=-1000000,tScore;
	for (int y = 1; y <= MAPHEIGHT; y++)
		for (int x = 1; x <= MAPWIDTH; x++)
			for (int o = 0; o < 4; o++)
			{
                //if (blockTypeBegin==6) cout<<block.set(x, y, o).isValid()<<endl;
				if (block.set(x, y, o).isValid() && block.onGround() &&
					Util::checkDirectDropTo(currBotColor, block.blockType, x, y, o))
				{
                    tScore=giveScore2(x,y,o,nextTypeForColor[currBotColor],currBotColor);
                    //cout<<tScore<<"----"<<endl;
                    if (tScore>score)
                    {
                        finalX = x;
    					finalY = y;
    					finalO = o;
                        score=tScore;
                        //cout<<"233"<<endl;
                    }
				}
			}
    //Util::printField();
    goto determined;
determined:
	// 再看看给对方什么好

	int maxCount = 0, minCount = 99;
    // int giveTest[14]={2,6,2,6,5,0,3,0,3,1,5,4,1,4};
    // int offset=0;
    // for (int i=0;i<14;i++)
    // {
    //     if (blockTypeBegin==giveTest[i])
    //     {
    //         offset=i;
    //         break;
    //     }
    // }
    // blockForEnemy = giveTest[(turnID+offset)%14];
	for (int i = 0; i < 7; i++)
	{
		if (typeCountForColor[enemyColor][i] > maxCount)
			maxCount = typeCountForColor[enemyColor][i];
		if (typeCountForColor[enemyColor][i] < minCount)
			minCount = typeCountForColor[enemyColor][i],blockForEnemy=i;
	}
	if (maxCount - minCount == 2)
	{
		// 危险，找一个不是最大的块给对方吧
		for (int blockForEnemy1 = 0; blockForEnemy1 < 7; blockForEnemy1++)
			if (typeCountForColor[enemyColor][blockForEnemy1] != maxCount)
			{
                Tetris block(blockForEnemy1, 1-currBotColor);
                double score=1000000000,tScore;
                for (int y = 1; y <= MAPHEIGHT; y++)
                    for (int x = 1; x <= MAPWIDTH; x++)
                        for (int o = 0; o < 4; o++)
                        {
                            //if (blockTypeBegin==6) cout<<block.set(x, y, o).isValid()<<endl;
                            if (block.set(x, y, o).isValid() && block.onGround() &&
                                Util::checkDirectDropTo(1-currBotColor, block.blockType, x, y, o))
                            {
                                tScore=giveScore2(x,y,o,blockForEnemy1,1-currBotColor);
                                //cout<<tScore<<endl;
                                if (tScore<score)
                                {
                                    blockForEnemy=blockForEnemy1;
                                    score=tScore;
                                    //cout<<"233"<<endl;
                                }
                            }
                        }
            }
    //     if (typeCountForColor[enemyColor][2] != maxCount) blockForEnemy=2;
    //     if (typeCountForColor[enemyColor][3] != maxCount) blockForEnemy=3;
	}
	else
	{
        for (int blockForEnemy1 = 0; blockForEnemy1 < 7; blockForEnemy1++)
		{
            Tetris block(blockForEnemy1, 1-currBotColor);
            double score=1000000000,tScore;
            for (int y = 1; y <= MAPHEIGHT; y++)
                for (int x = 1; x <= MAPWIDTH; x++)
                    for (int o = 0; o < 4; o++)
                    {
                        //if (blockTypeBegin==6) cout<<block.set(x, y, o).isValid()<<endl;
                        if (block.set(x, y, o).isValid() && block.onGround() &&
                            Util::checkDirectDropTo(1-currBotColor, block.blockType, x, y, o))
                        {
                            tScore=giveScore2(x,y,o,blockForEnemy1,1-currBotColor);
                            //cout<<tScore<<endl;
                            if (tScore<score)
                            {
                                blockForEnemy=blockForEnemy1;
                                score=tScore;
                                //cout<<"233"<<endl;
                            }
                        }
                    }
        }
        // blockForEnemy=3;
	}
//-----------------------------------------------------------------
	// 决策结束，输出结果（你只需修改以上部分）

	cout << blockForEnemy << " " << finalX << " " << finalY << " " << finalO;

	return 0;
}
